// 1. String building (should be OK)
let t0 = time()
let s = ""
for i in range(20000) { s += "x" }
print("string concat:", time() - t0)

// 2. Function calls in a loop (this is where tree-walkers die)
fn add(a, b) { return a + b }
let t0 = time()
let sum = 0
for i in range(200000) { sum = add(sum, i) }
print("fn calls:", time() - t0)

// 3. Array push in a loop (tests shared_ptr + map/index)
let t0 = time()
let a = []
for i in range(200000) { a.push(i) }
print("array push:", time() - t0)

// 4. Map access in a loop (tests hash path)
let m = { x: 1, y: 2, z: 3 }
let t0 = time()
let s = 0
for i in range(500000) { s += m.x }
print("map access:", time() - t0)