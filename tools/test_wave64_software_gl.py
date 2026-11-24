#!/usr/bin/env python3
"""Explicit Linux headless software GL differential test; no third-party Python packages."""
import argparse
import ctypes as C
import os
from pathlib import Path
import random
import json
import statistics
import time
import sys

U, I, P, Z = C.c_uint, C.c_int, C.c_void_p, C.c_ssize_t
M = 0xffffffff


def run(shader_name, seed, repeats=1):
    os.environ['LIBGL_ALWAYS_SOFTWARE'] = '1'
    egl = C.CDLL('libEGL.so.1')

    def bind(name, ret, *args):
        f = getattr(egl, name)
        f.restype, f.argtypes = ret, args
        return f

    proc = bind('eglGetProcAddress', P, C.c_char_p)

    def gl(name, ret, *args):
        addr = proc(name.encode())
        if not addr:
            raise RuntimeError('missing ' + name)
        return C.CFUNCTYPE(ret, *args)(addr)

    display = gl('eglGetPlatformDisplayEXT', P, U, P, P)(0x31dd, None, None)
    init = bind('eglInitialize', U, P, C.POINTER(I), C.POINTER(I))
    major, minor = I(), I()
    if not display or not init(display, C.byref(major), C.byref(minor)):
        raise RuntimeError('surfaceless EGL unavailable')
    context = None
    program = shader = 0
    query = U()
    buffers = (U * 2)()
    try:
        if not bind('eglBindAPI', U, U)(0x30a2):
            raise RuntimeError('OpenGL API unavailable')
        attrs = (I * 7)(0x3040, 8, 0x3033, 1, 0x3024, 8, 0x3038)
        config, n = P(), I()
        if not bind('eglChooseConfig', U, P, P, P, I, P)(display, attrs, C.byref(config), 1, C.byref(n)) or not n.value:
            raise RuntimeError('EGL config unavailable')
        attrs = (I * 7)(0x3098, 4, 0x30fb, 3, 0x30fd, 1, 0x3038)
        context = bind('eglCreateContext', P, P, P, P, P)(display, config, None, attrs)
        make_current = bind('eglMakeCurrent', U, P, P, P, P)
        if not context or not make_current(display, None, None, context):
            raise RuntimeError('OpenGL 4.3 context unavailable')
        get_string = gl('glGetString', C.c_char_p, U)
        renderer = get_string(0x1f01).decode()
        print('renderer=' + renderer, 'version=' + get_string(0x1f02).decode())
        if not any(x in renderer.lower() for x in ('llvmpipe', 'softpipe', 'swrast')):
            raise RuntimeError('not a confirmed software renderer')
        source = (Path(__file__).resolve().parents[1] / 'fixtures' / shader_name).read_bytes()
        shader = gl('glCreateShader', U, U)(0x91b9)
        s = C.c_char_p(source)
        gl('glShaderSource', None, U, I, P, P)(shader, 1, C.byref(s), None)
        gl('glCompileShader', None, U)(shader)
        ok = I()
        gl('glGetShaderiv', None, U, U, P)(shader, 0x8b81, C.byref(ok))
        if not ok.value:
            log = C.create_string_buffer(16384)
            gl('glGetShaderInfoLog', None, U, I, P, P)(shader, len(log), None, log)
            raise RuntimeError(log.value.decode())
        program = gl('glCreateProgram', U)()
        gl('glAttachShader', None, U, U)(program, shader)
        gl('glLinkProgram', None, U)(program)
        gl('glGetProgramiv', None, U, U, P)(program, 0x8b82, C.byref(ok))
        if not ok.value:
            log = C.create_string_buffer(16384)
            gl('glGetProgramInfoLog', None, U, I, P, P)(program, len(log), None, log)
            raise RuntimeError(log.value.decode())
        gl('glUseProgram', None, U)(program)
        words = 8
        if shader_name == "native_wave64_partition.comp":
            from wave64_partition_vectors import vectors, WORDS
            masks, inp, expected = vectors(seed)
            words = WORDS
        else:
            rng = random.Random(seed)
            masks = [0, (1<<64)-1, 1<<31, 1<<32, 1<<63, M, M<<32, 0x8000000180000001]
            masks += [rng.getrandbits(64) for _ in range(2048)]
            inp, expected = [], []
            for g, mask in enumerate(masks):
                pred = (1<<64)-1 if g<8 else rng.getrandbits(64)
                vals = [M if g<8 else rng.getrandbits(32) for _ in range(64)]
                ballot = mask & pred
                total = sum(vals[i] for i in range(64) if mask>>i&1)&M
                count = prefix = 0
                for i, v in enumerate(vals):
                    active = (mask>>i)&1
                    p = (pred>>i)&1
                    src = [0,31,32,63,64,0xffffffff,rng.randrange(64)][(i+g)%7]
                    inp.extend((v,active,p,src))
                    valid = int(bool(active and src<64 and mask>>src&1))
                    expected.extend((active,valid,vals[src] if valid else 0,count if active else 0,prefix if active else 0,total if active else 0,ballot&M if active else 0,ballot>>32 if active else 0))
                    if active:
                        prefix = (prefix+v)&M
                        count += p
        data = (U*len(inp))(*inp)
        output = (U*len(expected))()
        gl('glGenBuffers', None, I, P)(2,buffers)
        target = 0x90d2
        bind_buffer = gl('glBindBuffer', None, U, U)
        buffer_data = gl('glBufferData', None, U, Z, P, U)
        bind_base = gl('glBindBufferBase', None, U, U, U)
        bind_buffer(target,buffers[0]);buffer_data(target,C.sizeof(data),data,0x88e4);bind_base(target,0,buffers[0])
        bind_buffer(target,buffers[1]);buffer_data(target,C.sizeof(output),None,0x88e8);bind_base(target,1,buffers[1])
        dispatch = gl('glDispatchCompute', None, U, U, U)
        barrier = gl('glMemoryBarrier', None, U)
        finish = gl('glFinish', None)
        get_data = gl('glGetBufferSubData', None, U, Z, Z, P)
        error = gl('glGetError', U)
        gl('glGenQueries', None, I, P)(1,C.byref(query))
        begin = gl('glBeginQuery', None, U, U)
        end = gl('glEndQuery', None, U)
        get_query = gl('glGetQueryObjectui64v', None, U, U, P)
        # Warm-up dispatch, outside samples; force completion before timing.
        dispatch(len(masks),1,1);barrier(0x2200);finish()
        timers, wall = [], []
        for _ in range(repeats):
            # Poison every output before dispatch to avoid accepting stale results.
            poison=U(0xdeadbeef)
            gl('glClearBufferData', None, U, U, U, U, P)(target,0x8236,0x8d94,0x1405,C.byref(poison))
            finish()
            start=time.perf_counter_ns()
            begin(0x88bf,query.value)
            dispatch(len(masks),1,1);barrier(0x2200)
            end(0x88bf)
            elapsed=C.c_uint64()
            get_query(query.value,0x8866,C.byref(elapsed));finish()
            wall.append(time.perf_counter_ns()-start);timers.append(elapsed.value)
            bind_buffer(target,buffers[1]);get_data(target,0,C.sizeof(output),output)
            err=error()
            if err: raise RuntimeError(f'GL error {err:#x}')
            for i, (actual, want) in enumerate(zip(output,expected)):
                if actual != want:
                    raise RuntimeError(f'group={i//(64*words)} lane={(i//words)%64} word={i%words}: {actual} != {want}')
        print(json.dumps({'shader':shader_name,'seed':seed,'renderer':renderer,'groups':len(masks),'repeats':repeats,'timer_ns':timers,'wall_ns':wall,'median_timer_ns':statistics.median(timers),'median_wall_ns':statistics.median(wall),'mismatches':0}))
        print(f'groups={len(masks)} lanes={len(masks)*64} words={len(expected)} mismatches=0')
    finally:
        if context:
            if query.value: gl('glDeleteQueries', None, I, P)(1,C.byref(query))
            if buffers[0]: gl('glDeleteBuffers', None, I, P)(2,buffers)
            if program: gl('glDeleteProgram', None, U)(program)
            if shader: gl('glDeleteShader', None, U)(shader)
            bind('eglMakeCurrent', U, P, P, P, P)(display,None,None,None)
            bind('eglDestroyContext', U, P, P)(display,context)
        bind('eglTerminate', U, P)(display)


if __name__ == '__main__':
    try:
        parser = argparse.ArgumentParser(description=__doc__)
        parser.add_argument("--shader", choices=("native_wave64_compute.comp", "native_wave64_pingpong.comp", "native_wave64_blockscan.comp", "native_wave64_partition.comp"), default="native_wave64_compute.comp")
        parser.add_argument("--seed", type=int, default=64)
        parser.add_argument("--repeats", type=int, choices=range(1,21), default=1)
        args = parser.parse_args()
        run(args.shader, args.seed, args.repeats)
    except (OSError, RuntimeError) as e:
        print(str(e), file=sys.stderr)
        sys.exit(1)
