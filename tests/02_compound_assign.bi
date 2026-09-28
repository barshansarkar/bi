// member compound assignment: obj.foo.bar += 1
let counter = { hits: 0 }
let obj = { inner: counter }
obj.inner.hits += 5
assert(counter.hits == 0, "original counter must not be touched")
assert(obj.inner.hits == 5)

// index compound assignment
let arr = [10, 20, 30]
arr[0] += 5
assert(arr[0] == 15)

// string +=
let s = "ab"
s += "cd"
assert(s == "abcd")