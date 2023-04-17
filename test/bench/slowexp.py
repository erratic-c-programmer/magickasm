x = 93242345001
n = 5240582
m = 127382749
x %= m
a = 1
for i in range(n):
    a *= x
    a %= m

print(a)
