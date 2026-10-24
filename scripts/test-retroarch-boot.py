#!/usr/bin/env python3
"""Bounded macOS RetroArch boot probe; mutable files stay in --output/--system.

Adapted from SM2-Emu's smoke-retroarch.py configuration/process pattern.
Screenshots must be inspected before claiming game boot or gameplay.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import time

p = argparse.ArgumentParser(description=__doc__)
for key in ('retroarch', 'core', 'system', 'rom', 'output'):
    p.add_argument('--' + key, type=Path, required=True)
p.add_argument('--memory', choices=('64', '128'), default='64',
               help='Use 128 for a debug BIOS; retail Xbox defaults to 64 MB')
p.add_argument('--renderer', choices=('opengl', 'vulkan'), default='opengl')
p.add_argument('--vsync', choices=('on', 'off'), default='on',
               help='Diagnostic frontend VSync control; does not change core pacing')
p.add_argument('--video-scale', choices=('1', '2'), default='2')
p.add_argument('--swapchain-images', choices=('2', '3'),
               help='Diagnostic frontend swapchain image count')
p.add_argument('--sample-at', type=float,
               help='Capture a five-second macOS process sample after this many seconds')
p.add_argument('--timeout', type=int, default=180)
p.add_argument('--max-frames', type=int, default=1800)
a = p.parse_args()
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=True)
cfg = {
    'system_directory': str(a.system.resolve()),
    'video_driver': 'vulkan' if a.renderer == 'vulkan' else 'glcore', 'audio_driver': 'coreaudio',
    'video_fullscreen': 'false', 'video_windowed_fullscreen': 'false',
    'video_scale': a.video_scale, 'video_vsync': 'true' if a.vsync == 'on' else 'false', 'video_threaded': 'false',
    'video_shader_enable': 'false', 'audio_sync': 'true',
    'pause_nonactive': 'false', 'config_save_on_exit': 'false',
    'bundle_assets_extract_enable': 'false', 'core_info_cache_enable': 'false',
    'content_history_enable': 'false', 'savestate_auto_save': 'false',
    'savestate_auto_load': 'false', 'auto_overrides_enable': 'false',
    'auto_remaps_enable': 'false', 'video_gpu_screenshot': 'true',
    'core_options_path': str(out / 'core-options.cfg'),
    'menu_driver': 'ozone', 'menu_show_start_screen': 'false',
    'network_cmd_enable': 'false', 'stdin_cmd_enable': 'false',
}
if a.swapchain_images is not None:
    cfg['video_max_swapchain_images'] = a.swapchain_images
for key, directory in [('savefile_directory', 'saves'),
                       ('savestate_directory', 'states'),
                       ('screenshot_directory', 'screenshots'),
                       ('playlist_directory', 'playlists'),
                       ('libretro_info_path', 'info'),
                       ('cache_directory', 'cache'),
                       ('log_dir', 'logs'),
                       ('rgui_config_directory', 'config')]:
    (out / directory).mkdir(exist_ok=True)
    cfg[key] = str(out / directory)
for key in ('content_history_path', 'content_favorites_path',
            'content_image_history_path', 'content_music_history_path',
            'content_video_history_path'):
    cfg[key] = str(out / (key + '.lpl'))
for value in cfg.values():
    if any(c in value for c in ('"', '\n', '\r')):
        raise ValueError('Unsupported configuration path character')
(out / 'retroarch.cfg').write_text(''.join(f'{k} = "{v}"\n' for k, v in cfg.items()))
(out / 'core-options.cfg').write_text(
    f'xemu_renderer = "{a.renderer}"\n'
    f'xemu_memory = "{a.memory}"\n'
    'xemu_network_backend = "disabled"\n')
command = [str(a.retroarch.resolve()), '-v', '-c', str(out / 'retroarch.cfg'),
           '-L', str(a.core.resolve()), '--max-frames', str(a.max_frames),
           '--max-frames-ss', '--max-frames-ss-path', str(out / 'final.png'),
           str(a.rom.resolve())]
manifest = dict(command=command, core_sha256=hashlib.sha256(a.core.read_bytes()).hexdigest(),
                system=str(a.system.resolve()), rom=str(a.rom.resolve()),
                timeout_seconds=a.timeout, max_frames=a.max_frames)
(out / 'command.json').write_text(json.dumps(manifest, indent=2) + '\n')
started = time.monotonic()
timed_out = False
with (out / 'run.log').open('w') as log:
    proc = subprocess.Popen(command, cwd=out, stdout=log, stderr=subprocess.STDOUT,
                            start_new_session=True)
    manifest['pid'] = proc.pid
    (out / 'process.json').write_text(json.dumps(manifest, indent=2) + '\n')
    try:
        if a.sample_at is not None:
            try:
                proc.wait(timeout=min(a.sample_at, a.timeout))
            except subprocess.TimeoutExpired:
                if time.monotonic() - started < a.timeout:
                    with (out / 'sample-command.log').open('w') as sample_log:
                        sample = subprocess.run(
                            ['/usr/bin/sample', str(proc.pid), '5', '-file', str(out / 'sample.txt')],
                            stdout=sample_log, stderr=subprocess.STDOUT,
                            timeout=min(30, max(1, a.timeout - (time.monotonic() - started))))
                    manifest['sample_exit_code'] = sample.returncode
        proc.wait(timeout=max(0.1, a.timeout - (time.monotonic() - started)))
    except subprocess.TimeoutExpired:
        timed_out = True
    finally:
        if proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=15)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()
result = dict(exit_code=proc.returncode, timed_out=timed_out,
              elapsed_seconds=round(time.monotonic() - started, 2),
              pid=proc.pid, process_reaped=True,
              screenshot_exists=(out / 'final.png').exists())
(out / 'result.json').write_text(json.dumps(result, indent=2) + '\n')
print(json.dumps(result))
raise SystemExit(0 if proc.returncode == 0 and not timed_out and result['screenshot_exists'] else 1)
