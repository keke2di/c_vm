import tkinter as tk
from tkinter import filedialog, scrolledtext, messagebox
import subprocess
import os
import sys
import threading
from pathlib import Path

class cVMGUI:
    def __init__(self, root):
        self.root = root
        self.project_root = Path(__file__).resolve().parent
        root.title("cVM - Python Code Protector")
        root.geometry("750x550")

        tk.Label(root, text="Python Source File to Protect:").pack(pady=5)

        self.file_frame = tk.Frame(root)
        self.file_frame.pack(fill=tk.X, padx=10)

        self.file_path = tk.StringVar()
        tk.Entry(self.file_frame, textvariable=self.file_path, width=60).pack(side=tk.LEFT, fill=tk.X, expand=True)
        tk.Button(self.file_frame, text="Browse", command=self.browse_file).pack(side=tk.RIGHT, padx=5)

        tk.Label(root, text="Output EXE Name:").pack(pady=5)
        self.output_name = tk.StringVar(value="protected_app")
        tk.Entry(root, textvariable=self.output_name, width=40).pack()

        self.build_btn = tk.Button(root, text="Build Protected EXE", command=self.build,
                                   bg="#2e7d32", fg="white", padx=20, pady=10, font=("Arial", 10, "bold"))
        self.build_btn.pack(pady=15)

        self.log = scrolledtext.ScrolledText(root, height=18, state='disabled', bg="#1e1e1e", fg="#d4d4d4")
        self.log.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)

        self.status = tk.Label(root, text="Ready", bd=1, relief=tk.SUNKEN, anchor=tk.W)
        self.status.pack(fill=tk.X, side=tk.BOTTOM)

    def browse_file(self):
        filename = filedialog.askopenfilename(filetypes=[("Python files", "*.py")])
        if filename:
            self.file_path.set(filename)

    def log_message(self, msg, level="INFO"):
        prefix = "[INFO] " if level == "INFO" else "[ERROR] " if level == "ERROR" else "[DEBUG] "
        def update():
            self.log.config(state="normal")
            self.log.insert(tk.END, prefix + msg + "\n")
            self.log.see(tk.END)
            self.log.config(state="disabled")
        self.root.after(0, update)

    def set_status(self, text):
        self.root.after(0, lambda: self.status.config(text=text))

    def build(self):
        py_file = self.file_path.get()
        if not py_file or not os.path.exists(py_file):
            messagebox.showerror("Error", "Please select a valid Python file.")
            return

        out_name = self.output_name.get().strip()
        if not out_name:
            out_name = "protected_app"

        self.build_btn.config(state='disabled')
        self.log_message("=== BUILD START ===")
        self.log_message(f"Input:  {py_file}")
        self.log_message(f"Output: {out_name}.exe")
        self.set_status("Building...")

        def build_thread():
            try:
                self._do_build(py_file, out_name)
            except Exception as e:
                self.log_message(f"UNHANDLED ERROR: {e}", "ERROR")
            finally:
                self.root.after(0, lambda: self.build_btn.config(state='normal'))
                self.root.after(0, lambda: self.set_status("Ready"))

        threading.Thread(target=build_thread, daemon=True).start()

    def _do_build(self, py_file, out_name):
        output_dir = self.project_root / "output"
        output_dir.mkdir(exist_ok=True)
        cvm_file = str(output_dir / f"{out_name}.cvm")

        self.log_message(f"Compiling to {cvm_file}...")
        self.set_status("Compiling...")

        cmd = [sys.executable, "-m", "compiler.cli", py_file, "-o", cvm_file]
        self.log_message(f"Running: {' '.join(cmd)}")
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=60, cwd=self.project_root)
            if result.returncode != 0:
                self.log_message("COMPILER ERROR:\n" + result.stderr, "ERROR")
                return
            self.log_message(result.stdout)
        except subprocess.TimeoutExpired:
            self.log_message("Compiler timed out", "ERROR")
            return
        except Exception as e:
            self.log_message(f"Compiler error: {e}", "ERROR")
            return

        self.log_message("=== COMPILE SUCCESS ===")

        stub_path = str(self.project_root / "vm_c" / "stub.exe")
        if not os.path.exists(stub_path):
            self.log_message("stub.exe not found. Building it now...")
            self.set_status("Building stub...")
            try:
                subprocess.run([sys.executable, str(self.project_root / "vm_c" / "build.py")], check=True, cwd=self.project_root)
            except Exception as e:
                self.log_message(f"Failed to build stub: {e}", "ERROR")
                return
            if not os.path.exists(stub_path):
                self.log_message("stub.exe still missing after build.", "ERROR")
                return

        exe_path = str(self.project_root / "output" / f"{out_name}.exe")
        self.log_message(f"Packing into {exe_path}...")
        self.set_status("Packing...")

        cmd = [sys.executable, "-m", "packer.pack", stub_path, cvm_file, exe_path]
        self.log_message(f"Running: {' '.join(cmd)}")
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30, cwd=self.project_root)
            if result.returncode != 0:
                self.log_message("PACKER ERROR:\n" + result.stderr, "ERROR")
                return
            self.log_message(result.stdout)
        except subprocess.TimeoutExpired:
            self.log_message("Packer timed out", "ERROR")
            return
        except Exception as e:
            self.log_message(f"Packer error: {e}", "ERROR")
            return

        self.log_message("=== BUILD COMPLETE ===")
        self.log_message(f"EXE created at: {os.path.abspath(exe_path)}")
        self.set_status("Done")

if __name__ == "__main__":
    root = tk.Tk()
    app = cVMGUI(root)
    root.mainloop()
