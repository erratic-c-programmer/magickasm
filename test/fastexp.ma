% Compute x^n mod m
put #151292348  % base
st #0 #7
put #8888888888888888888  % exp
st #0 #8
put #127382749  % mod
st #0 #9

ld #0 #7
mod $9
st #0 #7

put #1
st #0 #1

!exploop:
nop

ld #0 #8
mod #2

% if (exp % 2)
je !notset:
ld #0 #1
mul $7
mod $9
st #0 #1

% else
!notset:
nop
ld #0 #7
mul $7
mod $9
st #0 #7
ld #0 #8
div #2
st #0 #8
ja !exploop:

ld #0 #1
