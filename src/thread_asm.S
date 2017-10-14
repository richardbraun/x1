/*
 * Copyright (c) 2017 Richard Braun.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

.section .text
.code32

.global thread_load_context
thread_load_context:
  mov 4(%esp), %eax             /* Load the thread address */
  mov (%eax), %esp              /* Switch to the thread stack */
  jmp thread_restore_context

.global thread_switch_context
thread_switch_context:
  mov 4(%esp), %eax             /* Get prev thread address */
  mov 8(%esp), %ecx             /* Get next thread address */

  push %ebp                     /* Save registers owned by the caller */
  push %ebx
  push %edi
  push %esi

  mov %esp, (%eax)              /* Save prev thread stack pointer */
  mov (%ecx), %esp              /* Switch to the stack of the next thread */

thread_restore_context:
  pop %esi
  pop %edi
  pop %ebx
  pop %ebp

  ret
