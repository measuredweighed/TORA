# TORA

> "It began as a mistake" - Charles Bukowski

TORA is a typeless interpreted programming language implemented in C. It offers modern string-handling functionality, simplified dictionary and array manipulation and inline function declaration. The parser and interpreter were written largely as a learning exercise and are, as a result, probably useful to absolutely no-one. I did learn a lot, though. Mainly to never do this again.

Here’s what a TORA program to generate the Fibonacci sequence up to the 10th term looks like:

```js
func fib(n)
{
    if(n < 2)
    {
        return n;
    }

    return fib(n-1) + fib(n-2);
}

term = 1;
while(term <= 10)
{
    println("Term " + term + " of the Fibonacci sequence is " + fib(term));
    term = term + 1;
}
```

# Standard library
TORA comes with support for the following functions as part of it’s standard library:

<table>
	<tr>
		<td>*Math*</td>
		<td>sin, cos, tan, log, exp, round, min, max</td>
	</tr>
	<tr>
		<td>*Collections*</td>
		<td>length</td>
	</tr>
	<tr>
		<td>*I/O*</td>
		<td>print, println</td>
	</tr>
</table>

# Internal types
Internally TORA stores the value of numerics as `double`’s, with a separate expression available for the representation of `boolean` values. Negative numeric values are made possible by way of the `TORAParserNegativeUnaryExpression` expression.

# Collections
TORA’s array implementation behaves as a dynamically-sized map, offering both associative and indexed lookups with keys being either numeric or string-based. Values without associative keys will automatically have numeric indexes assigned to them.

# Memory management
TORA manages it’s AST (and resultant evaluation-time expressions) using a linked-list structure containing reference-counted objects managed via the `tora_retain` and `tora_release` methods.

During both the tokenisation and evaluation stages temporary objects are stored in queue structures. In the case of tokens, the queue is popped each time a fresh token is requested. Expressions generated as a result of the evaluation stage are drained after evaluation is complete.

# External libraries & mentions

TORA makes use of the excellent [exceptions4c](https://github.com/guillermocalvo/exceptions4c) library to handle exception handling. So watch out for exceptions. Because you’re gonna get exceptions.

TORA was greatly inspired by Mihai Bazon's excellent series "[How to implement a programming language](http://lisperator.net/blog/how-to-implement-a-programming-language/)".