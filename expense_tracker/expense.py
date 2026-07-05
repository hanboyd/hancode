import csv
import os
from datetime import datetime
from decimal import Decimal, InvalidOperation


# CSV 文件的表头，也是每条消费记录字典中使用的键。
FIELDNAMES = ["日期", "分类", "金额", "备注"]


def initialize_csv(file_path):
    """
    创建消费记录文件（仅在文件不存在时创建），并写入表头。
    参数 file_path：expense.csv 文件的完整路径。
    """
    if not os.path.exists(file_path):
        with open(file_path, "w", newline="", encoding="utf-8-sig") as csv_file:
            writer = csv.DictWriter(csv_file, fieldnames=FIELDNAMES)
            writer.writeheader()


def read_records(file_path):
    """
    从 CSV 文件读取所有消费记录，并以字典列表的形式返回。
    参数 file_path：expense.csv 文件的完整路径。
    """
    initialize_csv(file_path)
    with open(file_path, "r", newline="", encoding="utf-8-sig") as csv_file:
        reader = csv.DictReader(csv_file)
        return list(reader)


def input_date():
    """
    提示用户输入日期，检查格式后返回日期字符串。
    参数：无。
    """
    while True:
        date_text = input("请输入日期（YYYY-MM-DD）：").strip()
        try:
            datetime.strptime(date_text, "%Y-%m-%d")
            return date_text
        except ValueError:
            print("日期格式不正确，请输入例如 2026-07-05。")


def input_amount():
    """
    提示用户输入金额，检查金额有效且不小于零后返回 Decimal 对象。
    参数：无。
    """
    while True:
        amount_text = input("请输入金额：").strip()
        try:
            amount = Decimal(amount_text)
            if amount < 0:
                print("金额不能小于 0。")
                continue
            return amount
        except InvalidOperation:
            print("金额格式不正确，请输入数字，例如 25.80。")


def add_record(file_path):
    """
    获取用户输入的一条消费记录，并将记录追加到 CSV 文件。
    参数 file_path：expense.csv 文件的完整路径。
    """
    print("\n--- 添加消费记录 ---")
    date_text = input_date()

    category = input("请输入分类（例如餐饮、交通）：").strip()
    while not category:
        print("分类不能为空。")
        category = input("请输入分类（例如餐饮、交通）：").strip()

    amount = input_amount()
    note = input("请输入备注（可以留空）：").strip()

    # 每条消费记录都使用 dict 保存，键名与 CSV 表头一致。
    record = {
        "日期": date_text,
        "分类": category,
        "金额": f"{amount:.2f}",
        "备注": note,
    }

    initialize_csv(file_path)
    with open(file_path, "a", newline="", encoding="utf-8-sig") as csv_file:
        writer = csv.DictWriter(csv_file, fieldnames=FIELDNAMES)
        writer.writerow(record)

    print("记录添加成功！")


def view_records(file_path):
    """
    读取并用表格形式显示全部消费记录。
    参数 file_path：expense.csv 文件的完整路径。
    """
    print("\n--- 全部消费记录 ---")
    records = read_records(file_path)

    if not records:
        print("目前还没有消费记录。")
        return

    # 使用固定宽度输出表头和记录，不依赖第三方表格库。
    print(f"{'序号':<6}{'日期':<14}{'分类':<12}{'金额':>12}  备注")
    print("-" * 62)
    for index, record in enumerate(records, start=1):
        amount_text = f"￥{record['金额']}"
        print(
            f"{index:<6}{record['日期']:<14}"
            f"{record['分类']:<12}{amount_text:>12}  {record['备注']}"
        )


def category_statistics(file_path):
    """
    按分类累计消费金额，并显示每个分类的统计结果。
    参数 file_path：expense.csv 文件的完整路径。
    """
    print("\n--- 分类统计 ---")
    records = read_records(file_path)

    if not records:
        print("目前还没有消费记录。")
        return {}

    # 字典的键是分类名称，值是该分类累计的金额。
    totals_by_category = {}
    for record in records:
        category = record["分类"]
        try:
            amount = Decimal(record["金额"])
        except InvalidOperation:
            print(f"已跳过金额无效的记录：{record}")
            continue

        totals_by_category[category] = (
            totals_by_category.get(category, Decimal("0")) + amount
        )

    if not totals_by_category:
        print("没有可统计的有效记录。")
        return {}

    for category, total in totals_by_category.items():
        print(f"{category}: ￥{total:.2f}")

    return totals_by_category


def calculate_total(file_path):
    """
    计算并显示所有有效消费记录的总金额。
    参数 file_path：expense.csv 文件的完整路径。
    """
    print("\n--- 总消费 ---")
    records = read_records(file_path)
    total = Decimal("0")

    for record in records:
        try:
            total += Decimal(record["金额"])
        except InvalidOperation:
            print(f"已跳过金额无效的记录：{record}")

    print(f"总消费：￥{total:.2f}")
    return total


def show_menu():
    """
    在命令行中清晰显示主菜单选项。
    参数：无。
    """
    print(
        "\n"
        "========== 命令行记账工具 ==========\n"
        "1. 添加记录\n"
        "2. 查看全部\n"
        "3. 分类统计\n"
        "4. 总消费\n"
        "5. 退出\n"
        "===================================="
    )


def main():
    """
    初始化 CSV 文件并循环处理用户选择，直到用户退出。
    参数：无。
    """
    # 将 expense.csv 放在脚本所在文件夹，避免从不同目录运行时找不到数据。
    script_directory = os.path.dirname(os.path.abspath(__file__))
    file_path = os.path.join(script_directory, "expense.csv")
    initialize_csv(file_path)

    while True:
        show_menu()
        choice = input("请选择功能（1-5）：").strip()

        if choice == "1":
            add_record(file_path)
        elif choice == "2":
            view_records(file_path)
        elif choice == "3":
            category_statistics(file_path)
        elif choice == "4":
            calculate_total(file_path)
        elif choice == "5":
            print("已退出记账工具，再见！")
            break
        else:
            print("无效选择，请输入 1 到 5。")


if __name__ == "__main__":
    main()
