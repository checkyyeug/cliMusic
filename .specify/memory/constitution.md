<!--
 Sync Impact Report
 =================
 Version change: 0.0.0 -> 1.0.0 (initial ratification)

 Modified principles: N/A (initial creation)

 Added sections:
 - Core Principles (3 principles)
 - Development Standards
 - Quality Gates
 - Governance

 Removed sections: N/A

 Templates requiring updates:
 - .specify/templates/plan-template.md:  Updated Constitution Check section reference
 - .specify/templates/spec-template.md:    Already aligned (no principle-specific changes needed)
 - .specify/templates/tasks-template.md:   Already aligned (TDD discipline reflected in template)

 Follow-up TODOs: None
-->

# XPU 音乐播放系统 Constitution

## Core Principles

### I. 代码质量优先 (Code Quality First)

**规则**:
- 所有代码MUST遵循清晰的命名约定和编码规范
- 代码复杂度MUST保持在可管理范围内 (圈复杂度 < 15)
- 每个函数MUST职责单一 (单一职责原则)
- 代码MUST通过静态分析工具检查 (无警告)
- 所有公共接口MUST有清晰的文档注释

**理由**: 高质量代码是长期可维护性的基础。XPU作为模块化系统，各模块间的接口清晰度和代码质量直接影响系统的可靠性和可扩展性。

---

### II. 测试驱动开发 (Test-Driven Development) - NON-NEGOTIABLE

**规则**:
- TDD流程MUST严格遵循: 编写测试 -> 测试失败 -> 实现功能 -> 测试通过
- 每个新功能MUST先有测试用例，获得用户批准后方可实现
- Red-Green-Refactor循环MUST被强制执行
- 测试覆盖率MUST保持在80%以上
- 关键音频处理路径MUST有100%覆盖

**理由**: TDD确保代码行为符合预期，防止回归，并为重构提供安全网。对于音频系统这种对正确性要求极高的场景，测试是质量保障的核心。

---

### III. 性能与安全性 (Performance & Security)

**规则**:
- 所有音频处理模块MUST支持无损格式 (FLAC, WAV, ALAC, DSD)
- 播放延迟MUST低于100ms (实时音频要求)
- 内存使用MUST在合理范围内 (单个音频流 < 500MB)
- 所有外部输入MUST经过验证和清理
- 音频文件解析MUST防止缓冲区溢出
- 网络接口MUST使用加密传输 (TLS/WSS)
- 敏感数据MUST安全存储 (使用系统密钥链)

**理由**: 音频系统对性能敏感，任何延迟都会影响用户体验。同时，作为处理用户文件的系统，安全性是保护用户隐私和系统稳定性的关键。

---

## Development Standards

### 编码规范

- **C/C++代码**: 遵循 MISRA C 2012 (在合理范围内) 或 Google C++ Style Guide
- **Python代码**: 遵循 PEP 8，使用 black 格式化，pylint 检查
- **注释**: 关键算法MUST有注释说明，复杂逻辑MUST有示例

### 模块化要求

- 每个CLI模块MUST可独立运行
- 模块间通信MUST通过标准输入/输出或定义良好的API
- 循环依赖MUST被禁止
- 公共接口变更MUST经过审查

### 文档要求

- 每个模块MUST有README说明其用途和用法
- API变更MUST更新相关文档
- 复杂配置MUST有示例文件

---

## Quality Gates

### 提交前检查

- [ ] 所有测试通过 (单元测试 + 集成测试)
- [ ] 代码覆盖率未下降
- [ ] 静态分析无新增警告
- [ ] 代码已格式化
- [ ] 文档已更新 (如需要)

### 发布前检查

- [ ] 所有核心功能有集成测试
- [ ] 性能基准测试通过
- [ ] 安全审查完成
- [ ] 向后兼容性验证 (如适用)
- [ ] 快速指南 (quickstart.md) 可运行

---

## Governance

### 修订程序

1. 宪法修订MUST有明确的理由
2. 修订提案MUST包含影响分析
3. 修订MUST更新所有相关模板和文档
4. 版本号根据语义化版本规则递增

### 版本策略

- **MAJOR**: 删除或重新定义核心原则 (破坏性变更)
- **MINOR**: 新增原则或实质性扩展指导原则
- **PATCH**: 澄清、措辞改进、非语义优化

### 合规审查

- 所有PRMUST验证符合本宪法原则
- 复杂度引入MUST有明确理由记录
- 违反原则的设计MUST在Constitution Check中明确说明
- 日常工作参考 DESIGN.md 中的详细设计指导

---

**Version**: 1.0.0 | **Ratified**: 2026-01-07 | **Last Amended**: 2026-01-07
