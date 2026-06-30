# 引用 re：用于按中文分号“；”和英文分号“;”拆分批量输入的任务。
import re

# 引用 Path：用于稳定定位 todo.md，避免受运行命令所在目录影响。
from pathlib import Path

# 引用 List、Tuple：说明任务列表的数据结构，便于阅读函数参数和返回值。
from typing import List, Tuple


# 保存待办事项的 Markdown 文件。和 main.py 放在同一目录，方便项目整体移动。
TODO_FILE = Path(__file__).parent / "todo.md"

# 控制台分隔线。多个界面复用同一长度，保持输出风格一致。
LINE = "-" * 40


def ensure_todo_file() -> None:
    """功能：确保 todo.md 存在。

    参数：无。
    返回值：无。
    """
    # 文件不存在时才创建，避免覆盖用户已有任务。
    if not TODO_FILE.exists():
        TODO_FILE.write_text("", encoding="utf-8")


def load_tasks() -> List[Tuple[bool, str]]:
    """功能：从 todo.md 读取任务。

    参数：无。
    返回值：任务列表。每个任务是 (是否完成, 任务内容)。
    """
    ensure_todo_file()

    # tasks 是程序内部统一使用的数据结构，避免每个功能都直接处理 Markdown 字符串。
    tasks: List[Tuple[bool, str]] = []

    # splitlines() 去掉换行符，逐行判断 Markdown 任务状态。
    lines = TODO_FILE.read_text(encoding="utf-8").splitlines()
    for line in lines:
        # 只读取标准未完成任务行，其他 Markdown 内容暂不进入任务列表。
        if line.startswith("- [ ] "):
            tasks.append((False, line[6:]))
        # 兼容 [x] 和 [X]，避免用户手写 Markdown 时大小写造成识别失败。
        elif line.startswith("- [x] ") or line.startswith("- [X] "):
            tasks.append((True, line[6:]))

    return tasks


def save_tasks(tasks: List[Tuple[bool, str]]) -> None:
    """功能：把任务列表保存为 todo.md。

    参数：
        tasks：任务列表。格式为 (是否完成, 任务内容)。
    返回值：无。
    """
    lines = []

    for is_done, text in tasks:
        # 用内部 bool 状态统一生成 Markdown 复选框，保存格式保持一致。
        checkbox = "x" if is_done else " "
        lines.append(f"- [{checkbox}] {text}")

    # 用换行符拼成 Markdown 文件内容，便于 Git diff 和人工阅读。
    content = "\n".join(lines)
    # 非空文件末尾补一个换行，符合常见文本文件习惯。
    if content:
        content += "\n"

    TODO_FILE.write_text(content, encoding="utf-8")


def print_tasks(tasks: List[Tuple[bool, str]]) -> None:
    """功能：显示当前任务列表。

    参数：
        tasks：要显示的任务列表。
    返回值：无。
    """
    print("当前任务")
    print(LINE)

    # 没有任务时提前提示，避免用户继续使用完成/删除功能却不知道原因。
    if not tasks:
        print("暂无任务。")
        print("提示：标记完成、删除任务功能当前不可用，请先添加任务。")
        return

    for number, (is_done, text) in enumerate(tasks, start=1):
        # 编号从 1 开始显示，更符合用户输入习惯；内部列表索引后续再转换。
        status = "x" if is_done else " "
        print(f"{number:>2}. [{status}] {text}")


def print_menu(has_tasks: bool) -> None:
    """功能：显示功能菜单。

    参数：
        has_tasks：当前是否有任务，用来决定完成/删除是否可用。
    返回值：无。
    """
    print()
    print("功能列表")
    print(LINE)
    print("1. 添加任务")
    print("2. 查看任务")

    # 完成和删除必须依赖已有任务；无任务时仍显示菜单项，但标注不可用。
    if has_tasks:
        print("3. 标记任务完成")
        print("4. 删除任务")
    else:
        print("3. 标记任务完成（不可用：暂无任务）")
        print("4. 删除任务（不可用：暂无任务）")

    print("0. 退出")
    print(LINE)


def print_result(message: str) -> None:
    """功能：显示一次操作的结果。

    参数：
        message：要展示给用户的结果说明。
    返回值：无。
    """
    print()
    print("操作结果")
    print(LINE)
    print(message)


def print_screen() -> bool:
    """功能：显示主界面，并返回当前是否有任务。

    参数：无。
    返回值：True 表示有任务，False 表示没有任务。
    """
    tasks = load_tasks()

    print()
    print("Markdown 待办事项管理器")
    print(LINE)
    print_tasks(tasks)
    print_menu(bool(tasks))

    # main() 复用这个结果，避免重复读取文件来判断功能是否可用。
    return bool(tasks)


def read_menu_choice() -> str:
    """功能：读取用户选择的功能序号。

    参数：无。
    返回值：用户输入的菜单序号字符串。
    """
    print("请选择功能序号，然后按回车：")
    return input("> ").strip()


def wait_for_enter() -> None:
    """功能：暂停，等待用户返回功能列表。

    参数：无。
    返回值：无。
    """
    print()
    input("按回车返回功能列表...")


def read_task_number(prompt: str) -> int | None:
    """功能：读取任务编号。

    参数：
        prompt：提示用户输入编号的说明文字。
    返回值：有效编号返回 int；输入不是数字时返回 None。
    """
    print(prompt)
    value = input("> ").strip()

    # 编号必须是纯数字；负数、字母、小数都不作为任务编号处理。
    if not value.isdigit():
        print_result("请输入有效的任务编号。")
        return None

    return int(value)


def split_task_text(text: str) -> List[str]:
    """功能：把添加任务输入拆成一条或多条任务。

    参数：
        text：用户输入的任务内容，可包含中文分号或英文分号。
    返回值：拆分后的任务内容列表，空白片段会被忽略。
    """
    # 正则一次支持两种分隔符；strip() 清理用户输入中常见的多余空格。
    return [item.strip() for item in re.split(r"[;；]", text) if item.strip()]


def add_task(text: str) -> None:
    """功能：添加单条或批量任务。

    参数：
        text：用户输入的任务内容。多条任务用“；”或“;”分隔。
    返回值：无。
    """
    new_tasks = split_task_text(text)

    # 拆分后仍为空，说明用户只输入了空格或分号，不能保存为任务。
    if not new_tasks:
        print_result("任务内容不能为空。")
        return

    tasks = load_tasks()
    for task_text in new_tasks:
        # 新增任务默认未完成，所以状态统一写 False。
        tasks.append((False, task_text))

    save_tasks(tasks)

    # 单条和多条使用不同提示，让用户明确是否触发了批量添加。
    if len(new_tasks) == 1:
        print_result(f"已添加任务：{new_tasks[0]}")
    else:
        print_result(f"已添加 {len(new_tasks)} 条任务：{'；'.join(new_tasks)}")


def done_task(number: int) -> None:
    """功能：按编号把任务标记为完成。

    参数：
        number：用户看到的任务编号，从 1 开始。
    返回值：无。
    """
    tasks = load_tasks()
    # 用户看到的编号从 1 开始，列表索引从 0 开始，所以这里统一转换。
    index = number - 1

    # 防止用户输入 0 或超出范围的编号，避免修改错误任务。
    if index < 0 or index >= len(tasks):
        print_result(f"任务编号 {number} 不存在。")
        return

    _, text = tasks[index]
    tasks[index] = (True, text)
    save_tasks(tasks)
    print_result(f"已完成任务：{text}")


def delete_task(number: int) -> None:
    """功能：按编号删除任务，并显示删除后的列表。

    参数：
        number：用户看到的任务编号，从 1 开始。
    返回值：无。
    """
    tasks = load_tasks()
    # 将用户编号转换为列表索引，保持编号输入对用户友好。
    index = number - 1

    # 删除是不可逆操作，所以必须先确认编号确实存在。
    if index < 0 or index >= len(tasks):
        print_result(f"任务编号 {number} 不存在。")
        return

    _, text = tasks.pop(index)
    save_tasks(tasks)
    print_result(f"已删除任务：{text}")
    print()
    print_tasks(tasks)


def main() -> None:
    """功能：程序主入口，负责菜单循环和功能分发。

    参数：无。
    返回值：无。

    argparse 说明：
        当前版本采用交互式菜单，不使用 argparse。
        这样设计是为了让用户运行 main.py 后先看到任务和功能列表，再按序号选择。
    """
    while True:
        has_tasks = print_screen()
        choice = read_menu_choice()

        # 菜单序号直接映射到功能，保持控制台操作简单明确。
        if choice == "1":
            print("请输入任务内容：")
            text = input("> ").strip()
            add_task(text)
            wait_for_enter()
        elif choice == "2":
            print_result("任务已在上方列出。")
            wait_for_enter()
        elif choice == "3":
            # 没有任务时不进入编号输入，避免用户面对一个无法选择的空列表。
            if not has_tasks:
                print_result("当前没有任务，无法标记完成。请先添加任务。")
            else:
                number = read_task_number("请输入要标记完成的任务编号：")
                # None 表示编号输入无效，此时不调用完成逻辑。
                if number is not None:
                    done_task(number)
            wait_for_enter()
        elif choice == "4":
            # 删除任务必须依赖已有任务；没有任务时直接给出原因。
            if not has_tasks:
                print_result("当前没有任务，无法删除。请先添加任务。")
            else:
                number = read_task_number("请输入要删除的任务编号：")
                # None 表示编号输入无效，此时不调用删除逻辑。
                if number is not None:
                    delete_task(number)
            wait_for_enter()
        elif choice == "0":
            print_result("已退出。")
            break
        else:
            print_result("无效选择，请重新输入。")
            wait_for_enter()


# 直接运行 main.py 时启动程序；被测试脚本 import 时不会自动进入交互循环。
if __name__ == "__main__":
    main()
