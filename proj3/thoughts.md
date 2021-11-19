## Integer Overflow Detection
* Monitor the most significant bit, if it changes by adding or subtracting numbers of the same sign, or when shifting a postive number, this is an indication of overflow
* Overflow can't occur when the sign of 2 addition operands are different, or the sign of 2 subtraction operands are the same
* For unsigned integers you can use the carry flag instead of the overflow flag since there is no concept of sign
* Always perform the overflow detection before the arithmetic, because there is no way to inspect a value after the fact and tell it was the result of overflow
* Implement the arithmetic operators so the perform overflow detection before the arithmetic operation and have them throw an exception if there is overflow