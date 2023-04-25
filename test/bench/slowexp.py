x = 932
n = 5240582
m = 1273823
x %= m
a = 1
for i in range(n):
    a *= x
    a %= m

print(a)
