import unittest
import inspect
import clownfish

class MyTest(unittest.TestCase):

    def testTrue(self):
        self.assertTrue(True, "True should be true")

    def testClassesPresent(self):
        self.assertIsInstance(clownfish.Hash, type)
        self.assertTrue(inspect.ismethoddescriptor(clownfish.Hash.store))

if __name__ == '__main__':
    unittest.main()

