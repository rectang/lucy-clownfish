package main

import "flag"
import "fmt"
import "log"
import "os"
import "os/exec"

var charmonizerC   string = "../common/charmonizer.c"
var charmonizerEXE string = "charmonizer"
var charmonyH      string = "charmony.h"

func main() {
    flag.Parse()
    for _, command := range flag.Args() {
        switch command {
        case "build":
            build()
        case "clean":
            clean()
        default:
            log.Fatalf("Unrecognized target: %s", command)
        }
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
    return destInfo.ModTime().Before(origInfo.ModTime())
}


/*
func allCurrent(orig, dest []string) bool {
    var threshold time.Time
    var path string

    if len(dest) == 0 {
        return false
    }

    // Look for oldest destination path.
    for path := range dest {
        info, err := os.Stat(path)
        if os.IsNotExist(err) {
            // If any dest doesn't exist, we're not current.
            return false
        }
        if threshold == nil || info.ModTime.Before(threshold) {
            threshold = info.ModTime
        }
    }

    // If any source is newer than the oldest dest, we're not current.
    for path = range orig {
        info, err := os.Stat(path)
        if err != nil {
            if info.ModTime.After(threshold) {
                return false
            }
        }
    }

    // Current!
    return true
}
*/

func configure() {
    if !current(charmonizerC, charmonizerEXE) {
        command := exec.Command("cc", "-o", charmonizerEXE, charmonizerC)
        command.Stdout = os.Stdout
        command.Stderr = os.Stderr
        err := command.Run()
        if err != nil {
            log.Fatal(err)
        }
    }
    if !current(charmonizerEXE, charmonyH) {
        command := exec.Command("./charmonizer", "--cc=cc", "--enable-c",
                                "--enable-makefile", "--", "-std=gnu99",
                                "-O2")
        command.Stdout = os.Stdout
        command.Stderr = os.Stderr
        err := command.Run()
        if err != nil {
            log.Fatal(err)
        }
    }
}

func build() {
    configure()
    fmt.Println("Building")
}

func clean() {
    fmt.Println("Cleaning")
}
