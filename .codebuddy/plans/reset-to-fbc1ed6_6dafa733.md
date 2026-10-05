---
name: reset-to-fbc1ed6
overview: 将仓库回退到提交 fbc1ed6（初次），删除最新提交 6a0224f（改dap）。工作区干净，直接 git reset --hard。
todos:
  - id: git-reset-to-target
    content: 执行 git reset --hard fbc1ed69264ca579eac801084aafac67ee286704 回退并删除 6a0224f 提交
    status: completed
  - id: verify-git-log
    content: 用 git log --oneline 与 git status 验证 HEAD 指向 fbc1ed6 且工作区干净
    status: completed
    dependencies:
      - git-reset-to-target
---

## 用户需求
将本地 main 分支回退到指定提交 `fbc1ed69264ca579eac801084aafac67ee286704`（提交信息"初次"），并删除其后的最新提交 `6a0224f`（提交信息"改dap"）。

## 现状核实（已确认）
- 分支：main，工作区干净（nothing to commit, working tree clean）
- 提交历史：`6a0224f 改dap` → `fbc1ed6 初次` → `e5aaeaf 初次` → `280fc26 初次`
- 回退目标：HEAD 从 `6a0224f` 移到 `fbc1ed6`，即丢弃最新提交"改dap"的全部改动

## 执行策略
- 使用 `git reset --hard fbc1ed69264ca579eac801084aafac67ee286704` 一次性完成：移动 HEAD、重置暂存区、重置工作区
- 工作区当前干净，reset --hard 不会额外丢失未提交改动；被删除的提交仍可通过 reflog（`6a0224f`）找回，风险可控
- 执行后用 `git log --oneline -3` 与 `git status` 验证 HEAD 已指向目标提交且工作区干净

## 边界与注意
- 若 `6a0224f` 曾推送到远程，本地回退后推送需要 `--force-with-lease`；当前仅执行本地回退，不涉及远程操作
- 不做任何文件级修改，仅执行 git 历史操作
