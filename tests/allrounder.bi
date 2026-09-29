// ============================================================
//  .bi Allrounder Demo — সব feature এক ফাইলে
//  চালান: ./bi run tests/allrounder.bi
// ============================================================

print("=== 1. Variables & Types ===")
let name = "Barshan"
let age = 21
let pi = 3.14159
let isDev = true
let nothing = null

print("name:", name, "| type:", type(name))
print("age:", age, "| type:", type(age))
print("pi:", pi, "| type:", type(pi))
print("isDev:", isDev, "| type:", type(isDev))
print("nothing:", nothing, "| type:", type(nothing))

// ============================================================
print("\n=== 2. Arithmetic & Operators ===")
let a = 17
let b = 5
print("a + b =", a + b)
print("a - b =", a - b)
print("a * b =", a * b)
print("a / b =", a / b)
print("a % b =", a % b)
print("a > b:", a > b, "| a == b:", a == b, "| a != b:", a != b)
print("a > 10 && b < 10:", a > 10 && b < 10)
print("a < 10 || b < 10:", a < 10 || b < 10)
print("!isDev:", !isDev)

// compound assignment
let counter = 0
counter += 10
counter -= 3
counter *= 2
print("counter:", counter)

// ============================================================
print("\n=== 3. Strings & Methods ===")
let s = "  Hello, Bi Language!  "
print("original:     [" + s + "]")
print("trim:         [" + s.trim() + "]")
print("upper:        " + s.trim().upper())
print("lower:        " + s.trim().lower())
print("length:       " + str(s.trim().length))
print("contains Bi:  " + str(s.contains("Bi")))
print("startsWith H:" + str(s.trim().startsWith("H")))
print("endsWith !:   " + str(s.trim().endsWith("!")))
print("indexOf Bi:   " + str(s.indexOf("Bi")))
print("slice 2..7:   " + s.trim().slice(2, 7))
print("replace:      " + s.trim().replace("Bi", "🚀"))
print("repeat 3:     " + "ab".repeat(3))

let csv = "apple,banana,cherry,date"
let parts = csv.split(",")
print("split:", parts)
print("joined:", parts.join(" | "))

// ============================================================
print("\n=== 4. Arrays & Methods ===")
let nums = [5, 2, 8, 1, 9, 3]
print("nums:", nums)
print("length:", nums.length)
print("first:", nums[0], "| last:", nums[nums.length - 1])

nums.push(42)
print("after push(42):", nums)
let popped = nums.pop()
print("popped:", popped, "| now:", nums)

print("contains 8:", nums.contains(8))
print("indexOf 8:", nums.indexOf(8))
print("slice 1..4:", nums.slice(1, 4))

let sorted = [5, 2, 8, 1, 9, 3]
sorted.sort()
print("sorted:", sorted)

let reversed = [1, 2, 3, 4, 5]
reversed.reverse()
print("reversed:", reversed)

let a1 = [1, 2]
let a2 = [3, 4]
print("concat:", a1.concat(a2))

// ============================================================
print("\n=== 5. Maps & Methods ===")
let user = {
    name: "Barshan",
    age: 21,
    city: "Kolkata",
    skills: ["C++", "Python", ".bi"]
}
print("user:", user)
print("user.name:", user.name)
print("user.skills:", user.skills)
print("keys:", user.keys())
print("values:", user.values())
print("has 'age':", user.has("age"))
print("has 'email':", user.has("email"))
print("get 'city':", user.get("city"))
print("get 'email' default:", user.get("email", "N/A"))

user.email = "barshan@example.com"
print("after adding email:", user.email)

user.remove("city")
print("after removing city, keys:", user.keys())

// ============================================================
print("\n=== 6. Control Flow ===")
print("-- if/else --")
let score = 85
if (score >= 90) {
    print("Grade: A")
} else if (score >= 80) {
    print("Grade: B")
} else if (score >= 70) {
    print("Grade: C")
} else {
    print("Grade: F")
}

print("-- while --")
let i = 0
while (i < 5) {
    print("  i =", i)
    i += 1
}

print("-- for --")
for (let j = 0; j < 5; j += 1) {
    print("  j =", j)
}

print("-- for-in over array --")
for x in [10, 20, 30] {
    print("  x =", x)
}

print("-- for-in over map keys --")
for k in {a: 1, b: 2, c: 3} {
    print("  key =", k)
}

print("-- break/continue --")
for k in range(10) {
    if (k == 3) { continue }
    if (k == 7) { break }
    print("  k =", k)
}

print("-- range() --")
print("  range(5):", range(5))
print("  range(2, 8):", range(2, 8))
print("  range(0, 10, 3):", range(0, 10, 3))

// ============================================================
print("\n=== 7. Functions ===")

fn greet(who) {
    return "Hello, " + who + "!"
}
print(greet("Barshan"))

fn add(x, y) { return x + y }
fn mul(x, y) { return x * y }
print("add(3,4) =", add(3, 4))
print("mul(3,4) =", mul(3, 4))

// default-ish via null check
fn power(base, exp) {
    if (exp == null) { exp = 2 }
    let r = 1
    for k in range(exp) { r *= base }
    return r
}
print("power(2)    =", power(2))
print("power(2, 8) =", power(2, 8))

// recursion
fn fib(n) {
    if (n < 2) { return n }
    return fib(n - 1) + fib(n - 2)
}
print("fib(15) =", fib(15))

fn factorial(n) {
    if (n <= 1) { return 1 }
    return n * factorial(n - 1)
}
print("factorial(10) =", factorial(10))

// ============================================================
print("\n=== 8. Closures ===")

fn makeCounter() {
    let count = 0
    return fn() {
        count += 1
        return count
    }
}

let c1 = makeCounter()
let c2 = makeCounter()
print("c1():", c1(), c1(), c1())
print("c2():", c2())
print("c1():", c1())

fn makeAdder(n) {
    return fn(x) { return x + n }
}
let add5  = makeAdder(5)
let add10 = makeAdder(10)
print("add5(3)  =", add5(3))
print("add10(3) =", add10(3))

// ============================================================
print("\n=== 9. Higher-Order Functions ===")

let numbers = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]

let doubled = numbers.map(fn(x) { return x * 2 })
print("doubled:", doubled)

let evens = numbers.filter(fn(x) { return x % 2 == 0 })
print("evens:", evens)

let total = numbers.reduce(fn(acc, x) { return acc + x }, 0)
print("sum via reduce:", total)

let squared = numbers.map(fn(x) { return x * x })
print("squares:", squared)

// chain
let result = numbers
    .filter(fn(x) { return x % 2 == 0 })
    .map(fn(x) { return x * x })
    .reduce(fn(acc, x) { return acc + x }, 0)
print("sum of squares of evens:", result)

// ============================================================
print("\n=== 10. Try / Catch / Throw ===")

fn safeDivide(x, y) {
    if (y == 0) {
        throw "division by zero!"
    }
    return x / y
}

try {
    print("10 / 2 =", safeDivide(10, 2))
} catch (e) {
    print("Error:", e)
}

try {
    print("10 / 0 =", safeDivide(10, 0))
} catch (e) {
    print("Caught:", e)
}

// custom error object
fn validateAge(age) {
    if (age < 0) {
        throw { error: "INVALID_AGE", message: "age cannot be negative", value: age }
    }
    return "OK"
}

try {
    validateAge(-5)
} catch (err) {
    print("Validation failed:", err.error, "-", err.message, "(value:", err.value, ")")
}

// ============================================================
print("\n=== 11. JSON ===")

let data = {
    title: "Bi Language",
    version: "0.6.0",
    features: ["fast", "simple", "web-ready"],
    author: { name: "Barshan", country: "India" },
    stars: 1200
}

let jsonText = toJson(data)
print("toJson:", jsonText)

let parsed = parseJson(jsonText)
print("parsed title:", parsed.title)
print("parsed features:", parsed.features)
print("parsed author name:", parsed.author.name)
print("parsed stars:", parsed.stars)

// ============================================================
print("\n=== 12. Built-in Utilities ===")

print("len('hello'):", len("hello"))
print("len([1,2,3,4]):", len([1, 2, 3, 4]))
print("len({a:1,b:2}):", len({a: 1, b: 2}))

print("int('42'):", int("42"))
print("num('3.14'):", num("3.14"))
print("str(42):", str(42))
print("bool(0):", bool(0))
print("bool(1):", bool(1))

print("type(42):", type(42))
print("type('hi'):", type("hi"))
print("type([1,2]):", type([1, 2]))
print("type({a:1}):", type({a: 1}))
print("type(null):", type(null))

print("range(5):", range(5))
print("range(0,20,5):", range(0, 20, 5))

print("split('a-b-c','-'):", split("a-b-c", "-"))
print("join(['x','y','z'], '+'):", join(["x", "y", "z"], "+"))
print("trim('  hi  '):", "[" + trim("  hi  ") + "]")
print("upper('hello'):", upper("hello"))
print("lower('WORLD'):", lower("WORLD"))
print("replace('foo bar', 'bar', 'baz'):", replace("foo bar", "bar", "baz"))

print("keys({a:1,b:2}):", keys({a: 1, b: 2}))
print("values({a:1,b:2}):", values({a: 1, b: 2}))

// reference semantics helpers
let orig = [1, 2, 3]
let shared = orig
let copied = clone(orig)
shared.push(4)
copied.push(99)
print("orig:      ", orig)
print("shared:    ", shared, "(same as orig:", same(orig, shared), ")")
print("copied:    ", copied, "(same as orig:", same(orig, copied), ")")

// ============================================================
print("\n=== 13. Mini Algorithms ===")

// Bubble sort
fn bubbleSort(arr) {
    let n = arr.length
    let work = clone(arr)
    for i in range(n) {
        for j in range(0, n - i - 1) {
            if (work[j] > work[j + 1]) {
                let tmp = work[j]
                work[j] = work[j + 1]
                work[j + 1] = tmp
            }
        }
    }
    return work
}
print("bubbleSort([64,34,25,12,22,11,90]):", bubbleSort([64, 34, 25, 12, 22, 11, 90]))

// Binary search
fn binarySearch(arr, target) {
    let lo = 0
    let hi = arr.length - 1
    while (lo <= hi) {
        let mid = int((lo + hi) / 2)
        if (arr[mid] == target) { return mid }
        if (arr[mid] < target)  { lo = mid + 1 }
        else                    { hi = mid - 1 }
    }
    return -1
}
let sortedArr = [1, 3, 5, 7, 9, 11, 13, 15]
print("binarySearch([...], 7):",  binarySearch(sortedArr, 7))
print("binarySearch([...], 15):", binarySearch(sortedArr, 15))
print("binarySearch([...], 8):",  binarySearch(sortedArr, 8))

// Word count
fn wordCount(text) {
    let words = text.lower().split(" ")
    let counts = {}
    for w in words {
        let w2 = w.trim()
        if (w2 != "") {
            counts[w2] = counts.get(w2, 0) + 1
        }
    }
    return counts
}
let sentence = "the quick brown fox jumps over the lazy dog the fox"
let counts = wordCount(sentence)
print("word counts:", counts)
print("'the' appears:", counts.get("the"))
print("'fox' appears:", counts.get("fox"))

// Palindrome
fn isPalindrome(s) {
    let cleaned = s.lower().replace(" ", "").replace(",", "").replace(".", "")
    let n = cleaned.length
    for i in range(int(n / 2)) {
        if (cleaned[i] != cleaned[n - 1 - i]) { return false }
    }
    return true
}
print("'A man a plan a canal Panama':", isPalindrome("A man a plan a canal Panama"))
print("'hello':", isPalindrome("hello"))

// Fibonacci list
fn fibList(n) {
    let out = []
    for i in range(n) {
        if (i < 2) { out.push(i) }
        else       { out.push(out[i - 1] + out[i - 2]) }
    }
    return out
}
print("fibList(10):", fibList(10))

// ============================================================
print("\n=== 14. Performance Snapshot ===")

let t0 = time()

let sum = 0
for i in range(1000000) {
    sum += i
}
let t1 = time()
print("1M loop:        ", t1 - t0, "s   (sum =" + str(sum) + ")")

let t2 = time()
fn inc(x) { return x + 1 }
let n = 0
for i in range(100000) { n = inc(n) }
let t3 = time()
print("100k fn calls:  ", t3 - t2, "s   (n =" + str(n) + ")")

let t4 = time()
let big = []
for i in range(50000) { big.push(i) }
let t5 = time()
print("50k array push: ", t5 - t4, "s   (len =" + str(big.length) + ")")

let t6 = time()
let m = {}
for i in range(50000) { m["k" + str(i)] = i }
let t7 = time()
print("50k map insert: ", t7 - t6, "s   (size =" + str(len(m)) + ")")

// ============================================================
print("\n=== 15. UTF-8 / Unicode ===")
let bangla = "আমার সোনার বাংলা"
print("bangla:", bangla)
print("length (chars):", bangla.length)
print("first char:", bangla.charAt(0))
print("first 5 chars:", bangla.slice(0, 5))

let emojis = "🚀🎯💡🔥"
print("emojis:", emojis)
print("count:", emojis.length)

// ============================================================
print("\n=== DONE — All systems checked ✓ ===")