#!/usr/bin/env python3
"""Private Xvfb/DBus/IBus integration test; requires python-xlib and libpinyin."""
import argparse
import os
from pathlib import Path
import select
import subprocess
import sys
import tempfile
import time


def session(probe):
    from Xlib import display, X, XK
    from Xlib.ext import xtest

    os.environ['IBUS_ADDRESS'] = 'unix:path=' + os.environ['XDG_RUNTIME_DIR'] + '/ibus-bus'
    children = []
    try:
        children.append(subprocess.Popen([
            'ibus-daemon', '--address=' + os.environ['IBUS_ADDRESS'], '--xim',
            '--panel=/usr/libexec/ibus-ui-gtk3', '--emoji-extension=disable'],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL))
        time.sleep(3)
        children.append(subprocess.Popen(['/usr/libexec/ibus-engine-libpinyin', '--ibus'],
                                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL))
        time.sleep(.5)
        app = subprocess.Popen([probe, '--interactive'], stdout=subprocess.PIPE, text=True)
        children.append(app)
        if not select.select([app.stdout], [], [], 5)[0]:
            raise RuntimeError('probe startup timeout')
        assert app.stdout.readline().strip() == 'interactive ready'
        connection = display.Display()
        root = connection.screen().root
        window = next(w for w in root.query_tree().children
                      if w.get_wm_name() == 'IME cursor probe')
        window.set_input_focus(X.RevertToParent, X.CurrentTime)
        connection.sync()
        subprocess.run(['/usr/bin/python3', '-c',
                        "import gi;gi.require_version('IBus','1.0');"
                        "from gi.repository import IBus;IBus.init();b=IBus.Bus();"
                        "assert b.is_connected();assert b.set_global_engine('libpinyin')"],
                       check=True, timeout=8)

        def key(name):
            code = connection.keysym_to_keycode(XK.string_to_keysym(name))
            xtest.fake_input(connection, X.KeyPress, code)
            xtest.fake_input(connection, X.KeyRelease, code)
            connection.sync()
            time.sleep(.08)

        def verify(caret):
            time.sleep(1)
            popup = next(w for w in root.query_tree().children
                         if w.get_wm_name() == 'ibus-ui-gtk3'
                         and w.get_attributes().map_state == X.IsViewable)
            origin = window.get_geometry()
            position = popup.get_geometry()
            expected = (origin.x + caret[0], origin.y + caret[1])
            actual = (position.x, position.y)
            assert actual == expected, (actual, expected)
            print('candidate', actual, 'caret', caret, flush=True)

        for char in 'nihao':
            key(char)
        verify((120, 360))
        key('Escape')
        key('F2')
        for char in 'nihao':
            key(char)
        verify((300, 200))
        key('space')
        time.sleep(.3)
        app.terminate()
        committed = app.communicate(timeout=3)[0]
        assert 'commit U+4F60' in committed and 'commit U+597D' in committed, committed
        print(committed, end='')
    finally:
        for child in reversed(children):
            if child.poll() is None:
                child.terminate()
                try:
                    child.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    child.kill()
                    child.wait()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('probe', help='built eui_linux_ime_cursor_probe')
    parser.add_argument('--xvfb', default='Xvfb')
    parser.add_argument('--session', action='store_true', help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.session:
        session(args.probe)
        return
    with tempfile.TemporaryDirectory(prefix='eui-ime-') as folder:
        env = os.environ.copy()
        for name, suffix in [('XDG_CACHE_HOME', 'cache'), ('XDG_CONFIG_HOME', 'config'),
                             ('XDG_RUNTIME_DIR', 'run')]:
            env[name] = str(Path(folder) / suffix)
            Path(env[name]).mkdir(mode=0o700)
        read_fd, write_fd = os.pipe()
        server = subprocess.Popen([args.xvfb, '-displayfd', str(write_fd), '-screen', '0',
                                   '1024x768x24', '-nolisten', 'tcp', '-noreset'],
                                  pass_fds=(write_fd,), env=env, stdout=subprocess.DEVNULL,
                                  stderr=subprocess.DEVNULL)
        os.close(write_fd)
        try:
            if not select.select([read_fd], [], [], 5)[0]:
                raise RuntimeError('Xvfb startup timeout')
            env['DISPLAY'] = ':' + os.read(read_fd, 32).decode().strip()
            env['XMODIFIERS'] = '@im=ibus'
            env['LC_ALL'] = 'C.UTF-8'
            subprocess.run(['dbus-run-session', '--', sys.executable, str(Path(__file__).resolve()),
                            str(Path(args.probe).resolve()), '--session'],
                           env=env, check=True, timeout=30)
        finally:
            os.close(read_fd)
            server.terminate()
            server.wait(timeout=5)


if __name__ == '__main__':
    main()
