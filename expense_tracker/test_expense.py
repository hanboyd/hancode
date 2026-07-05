import csv
from decimal import Decimal

import pytest

import expense


@pytest.fixture
def temp_csv(tmp_path):
    """创建临时 CSV 文件路径，并写入标准表头。"""
    file_path = tmp_path / "expense.csv"
    expense.initialize_csv(file_path)
    return file_path


@pytest.fixture
def sample_records():
    """提供多个测试都会使用的样本消费记录。"""
    return [
        {"日期": "2026-07-01", "分类": "餐饮", "金额": "12.50", "备注": "午餐"},
        {"日期": "2026-07-02", "分类": "交通", "金额": "8.00", "备注": "地铁"},
        {"日期": "2026-07-03", "分类": "餐饮", "金额": "20.00", "备注": "晚餐"},
    ]


@pytest.fixture
def write_records(temp_csv):
    """提供向临时 CSV 批量写入记录的辅助函数。"""

    def _write(records):
        with open(temp_csv, "a", newline="", encoding="utf-8-sig") as csv_file:
            writer = csv.DictWriter(csv_file, fieldnames=expense.FIELDNAMES)
            writer.writerows(records)
        return temp_csv

    return _write


@pytest.fixture
def add_record_with_input(temp_csv, monkeypatch):
    """提供模拟用户输入并调用添加记录功能的辅助函数。"""

    def _add(date, category, amount, note):
        answers = iter([date, category, str(amount), note])
        monkeypatch.setattr("builtins.input", lambda _prompt="": next(answers))
        expense.add_record(temp_csv)
        return expense.read_records(temp_csv)

    return _add


# 测试文件不存在时会自动创建，并且第一行是正确表头。
def test_initialize_csv_creates_file_with_header(tmp_path):
    file_path = tmp_path / "new_expense.csv"

    expense.initialize_csv(file_path)

    assert file_path.exists()
    with open(file_path, newline="", encoding="utf-8-sig") as csv_file:
        assert next(csv.reader(csv_file)) == expense.FIELDNAMES


# 测试添加一条记录后，CSV 中恰好新增一行数据。
def test_add_one_record_creates_new_row(add_record_with_input):
    records = add_record_with_input("2026-07-05", "餐饮", "25", "早餐")

    assert len(records) == 1
    assert records[0] == {
        "日期": "2026-07-05",
        "分类": "餐饮",
        "金额": "25.00",
        "备注": "早餐",
    }


# 测试 CSV 使用 UTF-8 编码，中文内容能正确保存和读取。
def test_csv_uses_utf8_encoding(add_record_with_input, temp_csv):
    add_record_with_input("2026-07-05", "餐饮", "18", "牛肉面")

    text = temp_csv.read_bytes().decode("utf-8-sig")

    assert "餐饮" in text
    assert "牛肉面" in text


# 测试日期、分类、金额和备注等正常数据都能正确写入。
def test_add_normal_record_saves_all_fields(add_record_with_input):
    records = add_record_with_input("2026-06-30", "购物", "99", "日用品")

    assert records[0]["日期"] == "2026-06-30"
    assert records[0]["分类"] == "购物"
    assert records[0]["金额"] == "99.00"
    assert records[0]["备注"] == "日用品"


# 测试金额为零时也能作为边界值正确写入。
def test_add_record_accepts_zero_amount(add_record_with_input):
    records = add_record_with_input("2026-07-05", "其他", "0", "免费")

    assert records[0]["金额"] == "0.00"


# 测试带小数的金额能保留为两位小数。
def test_add_record_accepts_decimal_amount(add_record_with_input):
    records = add_record_with_input("2026-07-05", "交通", "12.5", "公交")

    assert records[0]["金额"] == "12.50"


# 测试备注为空字符串时仍能正常添加记录。
def test_add_record_accepts_empty_note(add_record_with_input):
    records = add_record_with_input("2026-07-05", "餐饮", "10", "")

    assert records[0]["备注"] == ""


# 测试常见中文分类名称能原样保存。
@pytest.mark.parametrize("category", ["餐饮", "交通"])
def test_add_record_preserves_chinese_category(add_record_with_input, category):
    records = add_record_with_input("2026-07-05", category, "10", "")

    assert records[0]["分类"] == category


# 测试有数据时能读取全部记录及其内容。
def test_read_records_returns_all_data(write_records, sample_records):
    file_path = write_records(sample_records)

    assert expense.read_records(file_path) == sample_records


# 测试只有表头的 CSV 会返回空列表。
def test_read_empty_csv_returns_empty_list(temp_csv):
    assert expense.read_records(temp_csv) == []


# 测试读取到的记录数与实际写入条数一致。
def test_read_record_count_matches_written_rows(write_records, sample_records):
    file_path = write_records(sample_records)

    assert len(expense.read_records(file_path)) == len(sample_records)


# 测试同一分类的多条消费金额会正确累加。
def test_category_statistics_sums_same_category(write_records):
    file_path = write_records(
        [
            {"日期": "2026-07-01", "分类": "餐饮", "金额": "10.50", "备注": ""},
            {"日期": "2026-07-02", "分类": "餐饮", "金额": "20.00", "备注": ""},
        ]
    )

    totals = expense.category_statistics(file_path)

    assert totals["餐饮"] == Decimal("30.50")


# 测试多个分类会分别计算各自的消费金额。
def test_category_statistics_separates_categories(write_records, sample_records):
    file_path = write_records(sample_records)

    totals = expense.category_statistics(file_path)

    assert totals == {"餐饮": Decimal("32.50"), "交通": Decimal("8.00")}


# 测试只有一个分类时仍能正常返回统计结果。
def test_category_statistics_handles_single_category(write_records):
    file_path = write_records(
        [{"日期": "2026-07-01", "分类": "购物", "金额": "40.00", "备注": ""}]
    )

    assert expense.category_statistics(file_path) == {"购物": Decimal("40.00")}


# 测试查询不存在的分类时可以安全地得到零。
def test_missing_category_can_default_to_zero(write_records, sample_records):
    file_path = write_records(sample_records)
    totals = expense.category_statistics(file_path)

    assert totals.get("娱乐", Decimal("0")) == Decimal("0")


# 测试金额为零的记录不会改变该分类的累计结果。
def test_zero_amount_does_not_change_category_total(write_records):
    file_path = write_records(
        [
            {"日期": "2026-07-01", "分类": "餐饮", "金额": "15.00", "备注": ""},
            {"日期": "2026-07-02", "分类": "餐饮", "金额": "0.00", "备注": ""},
        ]
    )

    assert expense.category_statistics(file_path)["餐饮"] == Decimal("15.00")


# 测试多条记录的总消费计算正确。
def test_calculate_total_sums_multiple_records(write_records, sample_records):
    file_path = write_records(sample_records)

    assert expense.calculate_total(file_path) == Decimal("40.50")


# 测试只有一条记录时，总消费等于该条记录的金额。
def test_calculate_total_for_one_record(write_records):
    file_path = write_records(
        [{"日期": "2026-07-01", "分类": "交通", "金额": "12.50", "备注": ""}]
    )

    assert expense.calculate_total(file_path) == Decimal("12.50")


# 测试没有任何记录时，总消费为零。
def test_calculate_total_without_records_is_zero(temp_csv):
    assert expense.calculate_total(temp_csv) == Decimal("0")
