let nums = [1, 2, 3, 4, 5]

let doubled = nums.map(fn(n) { n * 2 })
print("doubled:", doubled)

let evens = nums.filter(fn(n) { n % 2 == 0 })
print("evens:", evens)

let total = nums.reduce(fn(acc, n) { acc + n }, 0)
print("sum:", total)

let user = { name: "Ada", age: 36, city: "London" }
print("name:", user.name)
print("keys:", user.keys())
print("length:", user.length)
