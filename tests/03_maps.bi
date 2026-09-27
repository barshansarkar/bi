let m = { b: 1, a: 2, c: 3 }
assert(m.length == 3, "length")
assert(m.keys().join(",") == "b,a,c", "insertion order")
assert(m.get("zz", 99) == 99, "default")
