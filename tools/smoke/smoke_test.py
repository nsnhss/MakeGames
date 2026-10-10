# smoke_test.py — MakeGames 自动化 GUI 冒烟测试
#
# ImGui 应用无原生控件树（UIA/pywinauto 不适用），采用「键盘/鼠标注入 + 全屏截图
# 方差校验 + 日志扫描」方案。坐标基于 playmode_ui.cpp 的固定布局常量计算。
#
# 用法（仓库根目录）：
#   tools\smoke\.venv\Scripts\python.exe tools\smoke\smoke_test.py            # 全部
#   ... smoke_test.py --only game                                            # 单项
#   ... smoke_test.py --keep-logs                                            # 保留日志（默认跑完即清）
#   ... smoke_test.py --clean                                                # 仅清理日志后退出
#
# 退出码：0 = 全过；1 = 有失败项。
import argparse
import json
import os
import re
import subprocess
import sys
import time

# 必须在 import pyautogui 之前声明 DPI 感知：
# 否则 pygetwindow 返回逻辑坐标、pyautogui 点击逻辑坐标、ImageGrab 截物理像素，
# 三者坐标系不一致（150% 缩放下点击全部打偏）。
import ctypes
ctypes.windll.shcore.SetProcessDpiAwareness(2)  # PER_MONITOR_DPI_AWARE

import pyautogui
import pygetwindow as gw
from PIL import Image, ImageChops, ImageGrab, ImageStat

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BUILD = os.path.join(REPO, "cmake-build-release")
DEMO = os.path.join(REPO, "demo_project")
SHOTS = os.path.join(REPO, "tools", "smoke", "shots")
# game.exe 需要导出布局（cwd 含 assets/shaders，见 export_game.cpp:267）；
# demo_project 是裸项目，冒烟时合成最小导出布局到 runtime_project/（gitignore）
GAME_RUNTIME = os.path.join(REPO, "tools", "smoke", "runtime_project")


def prepare_game_runtime():
    """demo_project + 引擎着色器 → runtime_project（模拟导出根）。"""
    import shutil
    if os.path.isdir(GAME_RUNTIME):
        shutil.rmtree(GAME_RUNTIME)
    shutil.copytree(DEMO, GAME_RUNTIME)
    shutil.copytree(os.path.join(REPO, "assets", "shaders"),
                    os.path.join(GAME_RUNTIME, "assets", "shaders"))


# 各 exe 的日志落点（cwd 相对 logs/ 或 exe 目录 logs/）
LOG_DIRS = {
    "game": os.path.join(GAME_RUNTIME, "logs"),
    "editor": os.path.join(BUILD, "editor", "logs"),
    "launcher": os.path.join(BUILD, "launcher", "logs"),
}

# 环境性白名单：本机无音频设备等非回归错误
LOG_ERROR_WHITELIST = [
    re.compile(r"\[AudioManager\] Failed to open audio device"),
]

pyautogui.FAILSAFE = False  # 自动化期间鼠标到角落不中断（脚本自身控制节奏）

# SDL 键盘状态依赖硬件扫描码；pyautogui.keybd_event 传 bScan=0 时 SDL 收不到，
# 必须显式传扫描码（已实证：bScan=0x01 的 Esc 生效，pyautogui.press('esc') 无效）
_USER32 = ctypes.windll.user32
_KEY_SCANCODES = {"esc": (0x1B, 0x01), "w": (0x57, 0x11), "a": (0x41, 0x1E),
                  "s": (0x53, 0x1F), "d": (0x44, 0x20),
                  "up": (0x26, 0x48), "down": (0x28, 0x50),
                  "left": (0x25, 0x4B), "right": (0x27, 0x4D),
                  "enter": (0x0D, 0x1C)}


def key_down(name):
    vk, sc = _KEY_SCANCODES[name]
    _USER32.keybd_event(vk, sc, 0, 0)


def key_up(name):
    vk, sc = _KEY_SCANCODES[name]
    _USER32.keybd_event(vk, sc, 2, 0)  # KEYEVENTF_KEYUP


def press_key(name, hold=0.05):
    key_down(name)
    time.sleep(hold)
    key_up(name)


def cleanup_logs():
    """删除冒烟产生的日志与崩溃转储（含历史遗留）。"""
    removed = 0
    for d in list(LOG_DIRS.values()) + [os.path.join(REPO, "logs"), DEMO,
                                        os.path.join(BUILD, "editor"),
                                        os.path.join(BUILD, "launcher")]:
        for name in ("engine.log", "crash.log"):
            p = os.path.join(d, name)
            if os.path.isfile(p):
                os.remove(p)
                removed += 1
    return removed


def scan_log(app):
    """扫描日志，返回 (路径, 未白名单的 ERROR 行列表)。日志不存在视为致命。"""
    path = os.path.join(LOG_DIRS[app], "engine.log")
    if not os.path.isfile(path):
        return path, ["<engine.log not found>"]
    errors = []
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            if "ERROR" not in line:
                continue
            if any(w.search(line) for w in LOG_ERROR_WHITELIST):
                continue
            errors.append(line.strip())
    return path, errors


def shot(name, win=None):
    """截图（给 win 则裁到窗口矩形），返回 (路径, 灰度标准差, PIL 图)。"""
    os.makedirs(SHOTS, exist_ok=True)
    if win:
        img = ImageGrab.grab(bbox=(win.left, win.top, win.right, win.bottom))
    else:
        img = ImageGrab.grab()
    path = os.path.join(SHOTS, name + ".png")
    img.save(path)
    std = ImageStat.Stat(img.convert("L")).stddev[0]
    return path, std, img


def diff_ratio(a, b):
    """两图平均绝对差占比（0~1），用于确认画面状态确实切换。"""
    d = ImageChops.difference(a.convert("L"), b.convert("L"))
    return ImageStat.Stat(d).mean[0] / 255.0


def launch(exe_rel, args, cwd):
    exe = os.path.join(BUILD, exe_rel)
    proc = subprocess.Popen([exe] + args, cwd=cwd)
    return proc


def find_window(title_candidates, timeout=15.0):
    """按标题子串找窗口；找不到返回 None。枚举期间其他窗口可能瞬亡，逐项容错。"""
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            wins = gw.getAllWindows()
        except Exception:
            time.sleep(0.3)
            continue
        for w in wins:
            try:
                t = w.title or ""
                if any(c in t for c in title_candidates) and w.width > 100:
                    _ = w.left  # 验证句柄有效
                    return w
            except Exception:
                continue
        time.sleep(0.3)
    return None


def activate(win):
    """AttachThreadInput 绕过 SetForegroundWindow 的前台限制。"""
    try:
        if win.isMinimized:
            win.restore()
        hwnd = win._hWnd
        fg = _USER32.GetForegroundWindow()
        cur_tid = ctypes.windll.kernel32.GetCurrentThreadId()
        fg_tid = _USER32.GetWindowThreadProcessId(fg, None)
        _USER32.AttachThreadInput(cur_tid, fg_tid, True)
        _USER32.SetForegroundWindow(hwnd)
        _USER32.BringWindowToTop(hwnd)
        _USER32.AttachThreadInput(cur_tid, fg_tid, False)
        time.sleep(0.5)
    except Exception:
        pass  # 焦点抢占失败不致命，点击仍会落到坐标


def rewin(title_candidates):
    """每步重新解析窗口句柄（防 1400 句柄失效）。"""
    return find_window(title_candidates, timeout=3)


def click_frac(win, fx, fy):
    """按窗口矩形比例点击（缩放不变，免边框换算）。
    比例由 playmode_ui.cpp 布局常量推出：客户区 1280x720，
    窗口 ≈ 客户区 + 左/右边框 16 + 标题栏 31 + 底边框 8。"""
    activate(win)
    x = win.left + int(win.width * fx)
    y = win.top + int(win.height * fy)
    print(f"    click @({x},{y}) win=({win.left},{win.top},{win.width}x{win.height})")
    pyautogui.moveTo(x, y, duration=0.2)
    time.sleep(0.3)  # 让 ImGui 收到 hover
    pyautogui.click(x, y)
    time.sleep(0.4)


def focus_window(win):
    """点标题栏中央抢前台焦点（键盘事件只进前台窗口；避开右侧最小化/关闭钮）。"""
    try:
        pyautogui.click(win.left + int(win.width * 0.4), win.top + 15)
        time.sleep(0.5)
    except Exception:
        pass


def keys(seq, hold=0.6):
    for k in seq:
        key_down(k)
        time.sleep(hold)
        key_up(k)
        time.sleep(0.2)


def close_app(proc, win=None):
    """优先 Alt+F4 优雅退出，超时强杀。"""
    if proc.poll() is not None:
        return
    if win:
        activate(win)
        pyautogui.hotkey("alt", "f4")
        try:
            proc.wait(timeout=6)
            return
        except subprocess.TimeoutExpired:
            pass
    proc.kill()
    proc.wait(timeout=5)


def check(name, ok, detail=""):
    print(f"  [{'PASS' if ok else 'FAIL'}] {name}" + (f" — {detail}" if detail else ""))
    return ok


# ---------------------------------------------------------------- game
GAME_TITLES = ["魔塔村物语", "MakeGames Runtime", "MakeGames"]


def smoke_game():
    """game.exe：标题 → Start New Game → WASD 移动 → Esc 暂停 → Status → 存档槽。"""
    print("[game] 启动 (cwd=runtime_project 合成导出布局)")
    prepare_game_runtime()
    proc = launch(os.path.join("src", "game.exe"), [], GAME_RUNTIME)
    ok = True
    win = None
    try:
        win = find_window(GAME_TITLES, timeout=20)
        ok &= check("窗口出现", win is not None)
        if not win:
            return False
        time.sleep(4)
        _, std, img_title = shot("game_1_title", win)
        ok &= check("标题画面渲染", std > 8.0, f"stddev={std:.1f}")

        # Start New Game：客户区 (640,341) → 窗口比例 (648/1296, 372/759)
        click_frac(win, 648 / 1296, 372 / 759)
        # 连拍 4 帧检测渲染是否存活（画面冻结时相邻帧 diff 恒 0）
        frames = []
        for i in range(4):
            time.sleep(1.5)
            w2 = rewin(GAME_TITLES)
            if w2:
                win = w2
            _, std, img = shot(f"game_2_ingame_{i}", win)
            frames.append(img)
        alive_diffs = [diff_ratio(frames[i], frames[i + 1]) for i in range(3)]
        print(f"    帧间 diff: {[f'{d:.4f}' for d in alive_diffs]}")
        img_game = frames[-1]
        ok &= check("进入游戏渲染", std > 8.0, f"stddev={std:.1f}")
        ok &= check("画面从标题切换", diff_ratio(img_title, img_game) > 0.01,
                    f"diff={diff_ratio(img_title, img_game):.3f}")

        focus_window(win)
        # 注意：不能 w/d/s/a 绕圈——回到原点 diff≈0 假阴性；玩家 (160,120) 右侧无碰撞，连按 d
        keys(["d", "d"], hold=0.7)  # player_movement_system: WASD/方向键
        time.sleep(1)
        win = rewin(GAME_TITLES) or win
        _, _, img_moved = shot("game_3_moved", win)
        ok &= check("WASD 移动生效（D2）", diff_ratio(img_game, img_moved) > 0.001,
                    f"diff={diff_ratio(img_game, img_moved):.4f}")

        focus_window(win)
        press_key("esc", hold=0.1)  # game_runtime.cpp:77 Esc 暂停
        time.sleep(1.5)
        win = rewin(GAME_TITLES) or win
        _, std, img_pause = shot("game_4_pause", win)
        ok &= check("暂停菜单弹出", diff_ratio(img_moved, img_pause) > 0.005,
                    f"diff={diff_ratio(img_moved, img_pause):.3f}")

        # 暂停菜单第 5 个按钮 Status：客户区 y=404 → (31+404)/759
        click_frac(win, 648 / 1296, 435 / 759)
        time.sleep(1.0)
        _, std, img_status = shot("game_5_status", win)
        ok &= check("Status 页切换（D6-4）", diff_ratio(img_pause, img_status) > 0.001,
                    f"diff={diff_ratio(img_pause, img_status):.4f}")
        press_key("esc", hold=0.1)
        time.sleep(0.8)
        press_key("esc", hold=0.1)  # 退回游戏
        time.sleep(1.0)

        # 存档链路（D2 save_system）：上面两次 Esc（Status→Playing→Paused）后菜单已在
        # None 页打开，直接点 Save → Slot 1（再按 Esc 反而会关掉菜单）
        win = rewin(GAME_TITLES) or win
        click_frac(win, 648 / 1296, 333 / 759)  # Save：客户区 y=302
        time.sleep(1.0)
        click_frac(win, 648 / 1296, 331 / 759)  # Slot 1：客户区 y≈300
        time.sleep(1.5)
        shot("game_6_saved", win)
        saves = [f for f in ("saves/save_001.json", "saves/save1.json",
                             "save1.json", "saves/slot1.json", "slot1.json")
                 if os.path.isfile(os.path.join(GAME_RUNTIME, f))]
        if not saves:  # 兜底：saves 目录下任意新文件
            sdir = os.path.join(GAME_RUNTIME, "saves")
            if os.path.isdir(sdir) and os.listdir(sdir):
                saves = os.listdir(sdir)
        ok &= check("存档文件生成（D2）", len(saves) > 0,
                    ",".join(saves) if saves else "未找到")
    finally:
        close_app(proc, rewin(GAME_TITLES) or win)
    _, errors = scan_log("game")
    ok &= check("日志无非白名单 ERROR", not errors, "; ".join(errors[:3]))
    return ok


# ---------------------------------------------------------------- editor
def smoke_editor():
    """editor.exe：带项目启动 → 布局加载 → 连续渲染 → 优雅退出。"""
    print("[editor] 启动 (arg=demo_project)")
    proc = launch(os.path.join("editor", "editor.exe"), [DEMO], REPO)
    win = None
    ok = True
    try:
        win = find_window(["MakeGames", "Editor", "物语"], timeout=25)
        ok &= check("窗口出现", win is not None)
        if not win:
            return False
        time.sleep(6)
        activate(win)
        time.sleep(1)
        _, std, _ = shot("editor_1_main", win)
        ok &= check("编辑器界面渲染", std > 15.0, f"stddev={std:.1f}")
        time.sleep(3)
        activate(win)
        time.sleep(1)
        _, std2, _ = shot("editor_2_stable", win)
        ok &= check("持续渲染稳定", std2 > 15.0, f"stddev={std2:.1f}")
    finally:
        close_app(proc, win)
    path, errors = scan_log("editor")
    ok &= check("日志无非白名单 ERROR", not errors, "; ".join(errors[:3]))
    if os.path.isfile(path):
        with open(path, encoding="utf-8", errors="replace") as f:
            content = f.read()
        ok &= check("首帧项目加载完成", "project load end" in content)
    return ok


# ---------------------------------------------------------------- launcher
def smoke_launcher():
    """launcher.exe：启动 → 界面渲染 → 零告警。"""
    print("[launcher] 启动")
    proc = launch(os.path.join("launcher", "launcher.exe"), [],
                  os.path.join(BUILD, "launcher"))
    win = None
    ok = True
    try:
        win = find_window(["MakeGames", "Launcher"], timeout=20)
        ok &= check("窗口出现", win is not None)
        if not win:
            return False
        time.sleep(4)
        _, std, _ = shot("launcher_1_home", win)
        ok &= check("主页渲染", std > 10.0, f"stddev={std:.1f}")
    finally:
        close_app(proc, win)
    _, errors = scan_log("launcher")
    ok &= check("日志无非白名单 ERROR", not errors, "; ".join(errors[:3]))
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--only", choices=["game", "editor", "launcher"])
    ap.add_argument("--keep-logs", action="store_true")
    ap.add_argument("--clean", action="store_true", help="仅清理日志后退出")
    args = ap.parse_args()

    n = cleanup_logs()
    print(f"[clean] 清理历史日志 {n} 个")
    if args.clean:
        return 0

    suites = {"game": smoke_game, "editor": smoke_editor, "launcher": smoke_launcher}
    targets = [args.only] if args.only else list(suites)
    results = {}
    for name in targets:
        try:
            results[name] = suites[name]()
        except Exception as e:
            print(f"  [FAIL] 异常: {e}")
            results[name] = False
        # 失败套件保留日志供排查；成功套件即时清理
        if not args.keep_logs and results[name]:
            cleanup_logs()

    print("\n===== 冒烟结果 =====")
    for name, ok in results.items():
        print(f"  {name:8s}: {'PASS' if ok else 'FAIL'}")
    if not args.keep_logs and all(results.values()):
        print(f"[clean] 收尾清理日志 {cleanup_logs()} 个")
    elif not args.keep_logs:
        print("[clean] 有失败项，日志保留供排查（--clean 可手动清理）")
    return 0 if all(results.values()) else 1


if __name__ == "__main__":
    sys.exit(main())
