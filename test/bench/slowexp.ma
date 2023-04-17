% Compute x^n mod m, naively
put #93242345001  % base
st #0 #7
put #5240582  % exp
st #0 #8
put #127382749  % mod
st #0 #9

ld #0 #7
mod $9
st #0 #7

put #1  % result
st #0 #1
put $8  % index
st #0 #2

!exploop nop

ld #0 #1
mul $7
mod $9
st #0 #1

ld #0 #2
sub #1
st #0 #2
ja !exploop

ld #0 #1
