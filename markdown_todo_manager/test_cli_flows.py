import os
import subprocess
import sys
import unittest
from pathlib import Path


PROJECT_DIR = Path(__file__).parent
MAIN_FILE = PROJECT_DIR / "main.py"
TODO_FILE = PROJECT_DIR / "todo.md"


class MarkdownTodoCliTest(unittest.TestCase):
    """End-to-end tests for the interactive CLI."""

    @classmethod
    def setUpClass(cls) -> None:
        cls.original_exists = TODO_FILE.exists()
        cls.original_todo = TODO_FILE.read_text(encoding="utf-8") if cls.original_exists else ""

    @classmethod
    def tearDownClass(cls) -> None:
        if cls.original_exists:
            TODO_FILE.write_text(cls.original_todo, encoding="utf-8")
        elif TODO_FILE.exists():
            TODO_FILE.unlink()

    def setUp(self) -> None:
        self.maxDiff = None

    def write_todo(self, content: str) -> None:
        TODO_FILE.write_text(content, encoding="utf-8")

    def read_todo(self) -> str:
        return TODO_FILE.read_text(encoding="utf-8")

    def run_app(self, *inputs: str) -> subprocess.CompletedProcess[str]:
        env = os.environ.copy()
        env["PYTHONIOENCODING"] = "utf-8"

        return subprocess.run(
            [sys.executable, str(MAIN_FILE)],
            cwd=PROJECT_DIR,
            input="\n".join(inputs) + "\n",
            text=True,
            encoding="utf-8",
            errors="replace",
            capture_output=True,
            timeout=5,
            env=env,
        )

    def assert_app_ok(self, result: subprocess.CompletedProcess[str]) -> None:
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_main_py_compiles(self) -> None:
        result = subprocess.run(
            [sys.executable, "-m", "py_compile", str(MAIN_FILE)],
            cwd=PROJECT_DIR,
            text=True,
            encoding="utf-8",
            errors="replace",
            capture_output=True,
        )

        self.assertEqual(result.returncode, 0, result.stderr or result.stdout)

    def test_startup_lists_existing_tasks_before_menu(self) -> None:
        self.write_todo("- [ ] 健身\n- [x] 阅读《君主论》\n")

        result = self.run_app("0")

        self.assert_app_ok(result)
        self.assertIn("Markdown 待办事项管理器", result.stdout)
        self.assertIn("当前任务", result.stdout)
        self.assertIn("1. [ ] 健身", result.stdout)
        self.assertIn("2. [x] 阅读《君主论》", result.stdout)
        self.assertIn("功能列表", result.stdout)
        self.assertNotIn("不可用：暂无任务", result.stdout)
        self.assertEqual(self.read_todo(), "- [ ] 健身\n- [x] 阅读《君主论》\n")

    def test_startup_empty_state_disables_task_actions(self) -> None:
        self.write_todo("")

        result = self.run_app("0")

        self.assert_app_ok(result)
        self.assertIn("暂无任务。", result.stdout)
        self.assertIn("标记完成、删除任务功能当前不可用", result.stdout)
        self.assertIn("3. 标记任务完成（不可用：暂无任务）", result.stdout)
        self.assertIn("4. 删除任务（不可用：暂无任务）", result.stdout)
        self.assertEqual(self.read_todo(), "")

    def test_add_single_task(self) -> None:
        self.write_todo("")

        result = self.run_app("1", "买牛奶", "", "0")

        self.assert_app_ok(result)
        self.assertIn("已添加任务：买牛奶", result.stdout)
        self.assertEqual(self.read_todo(), "- [ ] 买牛奶\n")

    def test_add_multiple_tasks_with_chinese_and_english_semicolons(self) -> None:
        self.write_todo("")

        result = self.run_app("1", "健身；看书; 写代码； 复盘 ", "", "0")

        self.assert_app_ok(result)
        self.assertIn("已添加 4 条任务：健身；看书；写代码；复盘", result.stdout)
        self.assertEqual(
            self.read_todo(),
            "- [ ] 健身\n- [ ] 看书\n- [ ] 写代码\n- [ ] 复盘\n",
        )

    def test_add_empty_or_separator_only_task_is_rejected(self) -> None:
        self.write_todo("")

        result = self.run_app("1", " ； ;  ", "", "0")

        self.assert_app_ok(result)
        self.assertIn("任务内容不能为空。", result.stdout)
        self.assertEqual(self.read_todo(), "")

    def test_mark_task_done_by_number(self) -> None:
        self.write_todo("- [ ] 健身\n- [ ] 看书\n")

        result = self.run_app("3", "2", "", "0")

        self.assert_app_ok(result)
        self.assertIn("已完成任务：看书", result.stdout)
        self.assertEqual(self.read_todo(), "- [ ] 健身\n- [x] 看书\n")

    def test_delete_task_by_number_and_show_updated_list(self) -> None:
        self.write_todo("- [ ] 健身\n- [x] 阅读《君主论》\n- [ ] 看书\n")

        result = self.run_app("4", "2", "", "0")

        self.assert_app_ok(result)
        self.assertIn("已删除任务：阅读《君主论》", result.stdout)
        self.assertIn("当前任务", result.stdout)
        self.assertIn("1. [ ] 健身", result.stdout)
        self.assertIn("2. [ ] 看书", result.stdout)
        self.assertEqual(self.read_todo(), "- [ ] 健身\n- [ ] 看书\n")

    def test_done_and_delete_are_blocked_when_empty(self) -> None:
        self.write_todo("")

        done_result = self.run_app("3", "", "0")
        delete_result = self.run_app("4", "", "0")

        self.assert_app_ok(done_result)
        self.assert_app_ok(delete_result)
        self.assertIn("当前没有任务，无法标记完成。请先添加任务。", done_result.stdout)
        self.assertIn("当前没有任务，无法删除。请先添加任务。", delete_result.stdout)
        self.assertEqual(self.read_todo(), "")

    def test_invalid_menu_and_task_numbers_do_not_change_file(self) -> None:
        self.write_todo("- [ ] 健身\n")

        result = self.run_app("abc", "", "3", "0", "", "4", "99", "", "0")

        self.assert_app_ok(result)
        self.assertIn("无效选择，请重新输入。", result.stdout)
        self.assertIn("任务编号 0 不存在。", result.stdout)
        self.assertIn("任务编号 99 不存在。", result.stdout)
        self.assertEqual(self.read_todo(), "- [ ] 健身\n")

    def test_non_numeric_task_number_is_rejected(self) -> None:
        self.write_todo("- [ ] 健身\n")

        result = self.run_app("4", "abc", "", "0")

        self.assert_app_ok(result)
        self.assertIn("请输入有效的任务编号。", result.stdout)
        self.assertEqual(self.read_todo(), "- [ ] 健身\n")

    def test_uppercase_x_is_loaded_as_done(self) -> None:
        self.write_todo("- [X] 已完成但大写\n- [ ] 未完成\n")

        result = self.run_app("0")

        self.assert_app_ok(result)
        self.assertIn("1. [x] 已完成但大写", result.stdout)
        self.assertIn("2. [ ] 未完成", result.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
