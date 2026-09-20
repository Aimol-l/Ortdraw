#pragma once

// 单节点执行状态（GraphExecutor::nodeFinished 以 int 传递）
enum class NodeStatus { Ok = 0, Failed = 1, Skipped = 2, Cancelled = 3 };
