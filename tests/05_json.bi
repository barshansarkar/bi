let v = parseJson("{\"a\":[1,2,3],\"b\":true}")
assert(v.a.length == 3)
assert(v.b == true)