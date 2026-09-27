let a = [1, 2, 3]
assert(a.length == 3, "length")
assert(a[0] == 1, "index")
assert([3,1,2].sort().join(",") == "1,2,3", "sort")
assert([1,2,3].map(fn(x){ return x*2 }).join(",") == "2,4,6", "map")
assert([1,2,3].filter(fn(x){ return x > 1 }).join(",") == "2,3", "filter")
assert([1,2,3].reduce(fn(a,x){ return a+x }, 0) == 6, "reduce")
