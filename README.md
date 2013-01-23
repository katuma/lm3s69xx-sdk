
This is the bare-bones SDK for LM369xx Cortex-M3 devices.

TI Driver code (BSD licensed) is taken verbatim from the TI SDK, with some glue
code around to create sufficient ANSI C-compatible environment (within limits
of memory and IO constraints).

It contains two examples, ```ex1.c``` is a demo of minimal working ansi c environment, ```ex2.c``` is a tiny lwip server.

Both will take a string as an input and return it reversed.

To build & test you'll need fairly recent qemu-arm-system, netcat, dnsmasq, tunctl (uml-utilities or something):

```
$ git clone https://github.com/katuma/lm3s69xx-sdk.git
$ cd lm3s69xx-sdk/
$ make CSS=~/CodeSourcery/Sourcery_CodeBench_Lite_for_ARM_EABI
$ sudo ./test.sh
```

```
Copyright (c) 2012-2013 Karel Tuma <karel.tuma@omsquare.com>
All rights reserved. 

Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
3. The name of the author may not be used to endorse or promote products
   derived from this software without specific prior written permission. 

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
OF SUCH DAMAGE.

```
