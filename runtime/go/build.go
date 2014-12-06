/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* Build "script" for Apache Clownfish runtime.
 */
package main

import "flag"
import "fmt"
import "io"
import "io/ioutil"
import "log"
import "os"
import "os/exec"
import "path"
import "path/filepath"
import "strings"
import "runtime"

import "git-wip-us.apache.org/repos/asf/lucy-clownfish.git/compiler/go/cfc"

var packageName string = "git-wip-us.apache.org/repos/asf/lucy-clownfish.git/runtime/go/clownfish"
var charmonizerC string = "../common/charmonizer.c"
var charmonizerEXE string = "charmonizer"
var charmonyH string = "charmony.h"
var buildDir string
var buildGO string
var configGO string
var installedLibPath string

func init() {
	_, buildGO, _, _ = runtime.Caller(1)
	buildDir = path.Dir(buildGO)
	configGO = path.Join(buildDir, "clownfish", "config.go")
	var err error
	installedLibPath, err = cfc.InstalledLibPath(packageName)
	if err != nil {
		log.Fatal(err)
	}
}

func main() {
	os.Chdir(buildDir)
	flag.Parse()
	action := "build"
	args := flag.Args()
	if len(args) > 0 {
		action = args[0]
	}
	switch action {
	case "build":
		build()
	case "clean":
		clean()
	case "test":
		test()
	case "install":
		install()
	default:
		log.Fatalf("Unrecognized action specified: %s", action)
	}
}

func current(orig, dest string) bool {
	destInfo, err := os.Stat(dest)
	if err != nil {
		if os.IsNotExist(err) {
			// If dest doesn't exist, we're not current.
			return false
		} else {
			log.Fatalf("Unexpected stat err: %s", err)
		}
	}

	// If source is newer than dest, we're not current.
	origInfo, err := os.Stat(orig)
	if err != nil {
		log.Fatalf("Unexpected: %s", err)
	}
	return origInfo.ModTime().Before(destInfo.ModTime())
}

func runCommand(name string, args ...string) {
	command := exec.Command(name, args...)
	command.Stdout = os.Stdout
	command.Stderr = os.Stderr
	err := command.Run()
	if err != nil {
		log.Fatal(err)
	}
}

func configure() {
	if !current(charmonizerC, charmonizerEXE) {
		runCommand("cc", "-o", charmonizerEXE, charmonizerC)
	}
	if !current(charmonizerEXE, charmonyH) {
		runCommand("./charmonizer", "--cc=cc", "--enable-c", "--host=go",
			"--enable-makefile", "--", "-std=gnu99", "-O2")
	}
}

func runCFC() {
	hierarchy := cfc.NewHierarchy("autogen")
	hierarchy.AddSourceDir("../core")
	hierarchy.Build()
	autogenHeader := "/* Auto-generated by build.go. */\n"
	coreBinding := cfc.NewBindCore(hierarchy, autogenHeader, "")
	modified := coreBinding.WriteAllModified(false)
	if modified {
		bindClasses()
		cBinding := cfc.NewBindC(hierarchy, autogenHeader, "")
		cBinding.WriteCallbacks()
		goBinding := cfc.NewBindGo(hierarchy)
		goBinding.SetHeader(autogenHeader)
		packageDir := path.Join(buildDir, "clownfish")
		goBinding.WriteBindings("Clownfish", packageDir)
		hierarchy.WriteLog()
	}
}

func bindClasses() {
	parcel := cfc.FetchParcel("Clownfish")
	hashBinding := cfc.NewGoClass(parcel, "Clownfish::Hash")
	cfc.RegisterGoClass(hashBinding)
}

func prep() {
	configure()
	runCFC()
	runCommand("make", "-j", "static")
	writeConfigGO()
	runCommand("go", "build", packageName)
}

func build() {
	prep()
	runCommand("go", "build", packageName)
}

func test() {
	prep()
	runCommand("go", "test", packageName)
}

func copyFile(source, dest string) {
	sourceFH, err := os.Open(source)
	if err != nil {
		log.Fatal(err)
	}
	defer sourceFH.Close()
	destFH, err := os.Create(dest)
	if err != nil {
		log.Fatal(err)
	}
	defer destFH.Close()
	_, err = io.Copy(destFH, sourceFH)
	if err != nil {
		log.Fatalf("io.Copy from %s to %s failed: %s", source, dest, err)
	}
}

func installHeaders() {
	coreDir := "../core"
	incDir := cfc.MainIncludeDir()
	doInstall := func(source string, info os.FileInfo, err error) error {
		if strings.HasSuffix(source, ".cfp") || strings.HasSuffix(source, ".cfh") {
			dest := path.Join(incDir, strings.TrimPrefix(source, coreDir))
			destDir := path.Dir(dest)
			if _, err := os.Stat(destDir); os.IsNotExist(err) {
				err = os.MkdirAll(destDir, 0755)
				if err != nil {
					log.Fatalf("Can't create dir '%s': %s", destDir, err)
				}
			}
			os.Remove(dest)
			copyFile(source, dest)
		}
		return nil
	}
	filepath.Walk("../core", doInstall)
}

func installStaticLib() {
	tempLibPath := path.Join(buildDir, "libclownfish.a")
	destDir := path.Dir(installedLibPath)
	if _, err := os.Stat(destDir); os.IsNotExist(err) {
		err = os.MkdirAll(destDir, 0755)
		if err != nil {
			log.Fatalf("Can't create dir '%s': %s", destDir, err)
		}
	}
	os.Remove(installedLibPath)
	copyFile(tempLibPath, installedLibPath)
}

func install() {
	prep()
	runCommand("go", "install", packageName)
	installHeaders()
	installStaticLib()
}

func writeConfigGO() {
	if current(buildGO, configGO) {
		return
	}
	content := fmt.Sprintf(
		"// Auto-generated by build.go, specifying absolute path to static lib.\n"+
			"package clownfish\n"+
			"// #cgo CFLAGS: -I%s/../core\n"+
			"// #cgo CFLAGS: -I%s\n"+
			"// #cgo CFLAGS: -I%s/autogen/include\n"+
			"// #cgo LDFLAGS: -L%s\n"+
			"// #cgo LDFLAGS: -lclownfish\n"+
			"import \"C\"\n",
		buildDir, buildDir, buildDir, buildDir)
	ioutil.WriteFile(configGO, []byte(content), 0666)
}

func clean() {
	if _, err := os.Stat(charmonizerEXE); os.IsNotExist(err) {
		return
	}
	fmt.Println("Cleaning")
	runCommand("make", "clean")
	cleanables := []string{charmonizerEXE, charmonyH, "Makefile", configGO}
	for _, file := range cleanables {
		err := os.Remove(file)
		if err == nil {
			fmt.Println("Removing", file)
		} else if !os.IsNotExist(err) {
			log.Fatal(err)
		}
	}
}
