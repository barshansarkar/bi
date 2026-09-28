// arrays are copied on `let`
let a = [1, 2, 3]
let b = a
b.push(4)
assert(a.length == 3, "a should be unchanged, got " + str(a.length))
assert(b.length == 4, "b should be 4, got " + str(b.length))

// maps are copied too
let m1 = { x: 1 }
let m2 = m1
m2.x = 99
assert(m1.x == 1, "m1.x should be 1, got " + str(m1.x))

// deep copy: nested arrays
let outer = { list: [1, 2] }
let copy  = outer
copy.list.push(3)
assert(outer.list.length == 2, "nested array must not alias")
assert(copy.list.length  == 3)

// plain reassignment `=` also copies
let c = [1]
let d = [2]
c = d
c.push(3)
assert(d.length == 1, "d must not be mutated by c.push")