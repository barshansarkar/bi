// for-in gives a fresh copy per iteration
let src = [[1], [2]]
for let row in src {
    row.push(99)
}
assert(src[0].length == 1, "for-in must not alias source element")
assert(src[1].length == 1)

// try/catch binds a copy
let payload = [1]
try {
    throw payload
} catch (e) {
    e.push(2)
}
assert(payload.length == 1, "catch binding must be a copy")