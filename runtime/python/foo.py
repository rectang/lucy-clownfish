import clownfish
num = clownfish.Integer32(42)
print("hovo")
print(num)
num.inc_refcount()
print(num.get_value())
print(clownfish.StringHelper.is_whitespace(32))
print(clownfish.StringHelper.is_whitespace(31))
err = clownfish.Err(mess="oops")
print(err.get_mess())
myhash = clownfish.Hash(capacity=10)
print("I'm an Obj:", myhash.is_a("Clownfish::Obj"))

class MyError(Exception):
    pass

clownfish.MyError = MyError

try:
    raise clownfish.MyError
except Exception as e:
    print("Caught", e)

try:
    myhash.store(key="foo", value="morp")
except TypeError as e:
    print("Caught error:", e)
print(myhash.fetch("foo"))
