"""Графики по результатам бенчмарка ЛР-1.

Использование:
    python scripts/plot_benchmark.py benchmark.csv

Для каждого атрибута строит три картинки (build / find / remove): по горизонтали n,
по вертикали время, по одной панели на порядок входных данных (random / sorted / reversed),
по одной линии на структуру. Для построения нужен matplotlib.
"""
import csv
import os
import sys
from collections import defaultdict

import matplotlib

matplotlib.use("Agg")  # без окна - сразу в файлы
import matplotlib.pyplot as plt

ORDERS = ["random", "sorted", "reversed"]
STRUCTURE_COLORS = {
    "AVL Tree": "tab:blue",
    "Linear Array": "tab:orange",
    "Unbalanced BST": "tab:green",
}
METRICS = [
    ("build_ms", "Построение дерева, мс"),
    ("find_us", "Точный поиск, мкс на запрос"),
    ("remove_us", "Удаление, мкс на запрос"),
]


def load(path):
    # data[attribute][metric][order][structure] -> список (n, value)
    data = defaultdict(lambda: defaultdict(lambda: defaultdict(lambda: defaultdict(list))))
    with open(path, newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            if row["skipped"] == "1":
                continue
            for metric, _ in METRICS:
                if metric == "build_ms" and "Linear" in row["structure"]:
                    continue  # у перебора нет построения
                data[row["attribute"]][metric][row["order"]][row["structure"]].append(
                    (int(row["n"]), float(row[metric])))
    return data


def safe_name(text):
    return "".join(c if c.isalnum() else "_" for c in text).strip("_")


def plot(data, out_dir):
    os.makedirs(out_dir, exist_ok=True)
    saved = []
    for attribute, metrics in data.items():
        for metric, title in METRICS:
            fig, axes = plt.subplots(1, 3, figsize=(15, 4.5), sharex=True, sharey=True)
            for ax, order in zip(axes, ORDERS):
                for structure, points in sorted(metrics[metric][order].items()):
                    points.sort()
                    ax.plot([p[0] for p in points], [p[1] for p in points],
                            marker="o", color=STRUCTURE_COLORS.get(structure), label=structure)
                ax.set_title(order)
                ax.set_xlabel("Количество элементов")
                ax.set_xscale("log")
                ax.set_yscale("log")
                ax.grid(True, which="both", alpha=0.3)
            axes[0].set_ylabel(title)
            axes[-1].legend()
            fig.suptitle(f"{attribute}: {title}")
            fig.supxlabel("Пропущенные замеры и их причины указаны в CSV.", fontsize=9)
            fig.tight_layout()
            path = os.path.join(out_dir, f"{safe_name(attribute)}_{metric}.png")
            fig.savefig(path, dpi=120)
            plt.close(fig)
            saved.append(path)
    return saved


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    csv_path = sys.argv[1]
    out_dir = sys.argv[2] if len(sys.argv) > 2 else os.path.splitext(csv_path)[0] + "_plots"
    for path in plot(load(csv_path), out_dir):
        print("saved", path)


if __name__ == "__main__":
    main()
