# Phobos DockedAircraftAutoAttack Phase 1 Contract

## 1. 本阶段目标

本阶段目标是为 `DockedAircraftAutoAttack` 停机坪自动索敌建立最小闭环契约，不进入实现。

目标行为：

- 当全局开关和 AircraftType 开关同时启用时，停靠在机场、停机坪或 `UnitReload=true` dock 上的 Aircraft 按固定周期扫描目标。
- 扫描只在停靠且安全的 Aircraft 上执行。
- 找到合法目标后，只派发标准 Aircraft 攻击任务，让既有 `AircraftClass` mission、Phobos Aircraft hooks、武器选择和返航流程继续负责后续行为。
- Phase 1 继续使用原生单一 `TechnoClass::Ammo`，不引入双弹仓。

当前源码基础：

- Aircraft 当前武器索引由 `TechnoExt::CurrentAircraftWeaponIndex` 承载，见 `src/Ext/Techno/Body.h` 与 `TechnoExt::ExtData::Serialize`。
- Aircraft 武器选择 wrapper 在 `src/Ext/Aircraft/Hooks.cpp::AircraftClass_SelectWeapon_Wrapper`。
- Aircraft 专用实际开火集中在 `src/Ext/Aircraft/Body.cpp::AircraftExt::FireWeapon`。
- Aircraft 自动目标基础已有 `src/Ext/Aircraft/Hooks.cpp::AircraftClass_GreatestThreat`，并已有 `Team / Airstrike / Spawned / Ammo` 排除模式。
- dock 判断可参考 `TechnoExt::HasAvailableDock` 与 `TechnoExt::HasRadioLinkWithDock`。

## 2. 非目标

本阶段不设计或实现以下内容：

- 不实现主副武器独立弹仓。
- 不新增 `AircraftWeaponAmmo.*`。
- 不修改 UI 或弹药 pip。
- 不修改 `WeaponTypeExt`，尤其不触碰当前已有本地改动的 `src/Ext/WeaponType/Body.h` 和 `src/Ext/WeaponType/Body.cpp`。
- 不新增 WeaponType 级 reload / ammo 配置。
- 不做 AircraftType 转换换挂载。
- 不做运行时虚拟武器表。
- 不做停机坪原地开火。
- 不做空中 deploy 切换挂载。
- 不做射界、FireArc 或复杂战术评分。
- 不影响 `Airstrike`、`Spawned`、`Team` aircraft。
- 不修改 PhobosFog 逻辑。
- 不新增 hook，不提交 git。

## 3. 新增 INI 字段

### 3.1 RulesExt 全局字段

```ini
[General]
DockedAircraftAutoAttack=false
DockedAircraftAutoAttack.Interval=15
```

字段契约：

- `DockedAircraftAutoAttack`
  - 类型：boolean。
  - 默认值：`false`。
  - 语义：全局总开关。为 `false` 时所有 AircraftType 级字段均不生效。
- `DockedAircraftAutoAttack.Interval`
  - 类型：integer，单位为帧。
  - 默认值：`15`。
  - 语义：全局默认扫描间隔。
  - 约束：实现时将小于等于 0 的值 clamp 到 `1`。

### 3.2 TechnoTypeExt / AircraftType 字段

```ini
[ORCA]
DockedAircraftAutoAttack=no
DockedAircraftAutoAttack.Range=0
DockedAircraftAutoAttack.Interval=-1
DockedAircraftAutoAttack.MinAmmo=1
DockedAircraftAutoAttack.WeaponOrder=0,1
DockedAircraftAutoAttack.DisableOnDeploy=false
```

字段契约：

- `DockedAircraftAutoAttack`
  - 类型：boolean。
  - 默认值：`false`。
  - 语义：该 AircraftType 是否启用停机坪自动索敌。
- `DockedAircraftAutoAttack.Range`
  - 类型：integer 或 leptons-compatible range，最终实现阶段按现有 `Leptons` / cell range 读取模式确认。
  - 默认值：`0`。
  - 语义：扫描范围，单位为格。
  - 决策：Phase 1 默认 `0` 表示不扫描，不回退到 `Sight`。这样可避免仅设置开关后产生过宽的隐式行为。
- `DockedAircraftAutoAttack.Interval`
  - 类型：integer，单位为帧。
  - 默认值：`-1`。
  - 语义：每类飞机扫描间隔。小于 0 时使用全局 `DockedAircraftAutoAttack.Interval`；等于 0 或正数在实现时 clamp 到至少 `1`。
- `DockedAircraftAutoAttack.MinAmmo`
  - 类型：integer。
  - 默认值：`1`。
  - 语义：原生 `TechnoClass::Ammo` 至少达到该值才允许自动出击。
  - 约束：Phase 1 只检查原生单 Ammo。
- `DockedAircraftAutoAttack.WeaponOrder`
  - 类型：integer list。
  - 默认值：`0,1`。
  - 语义：自动索敌测试武器的顺序。Phase 1 只允许 `0` 和 `1`，忽略其它索引或在读取时裁剪。
- `DockedAircraftAutoAttack.DisableOnDeploy`
  - 类型：boolean。
  - 默认值：`false`。
  - 语义：是否允许未来通过 deploy 切换单架飞机的自动索敌禁用状态。
  - Phase 1 决策：只预留字段和运行时状态契约，不接 deploy hook，不实现切换。

## 4. 修改文件预算

未来实现阶段建议修改文件：

- `src/Ext/Rules/Body.h`
  - 新增全局开关和全局 interval 字段。
- `src/Ext/Rules/Body.cpp`
  - 读取并序列化全局字段。
- `src/Ext/TechnoType/Body.h`
  - 新增 AircraftType 级配置字段。
- `src/Ext/TechnoType/Body.cpp`
  - 读取并序列化 AircraftType 级字段。
- `src/Ext/Techno/Body.h`
  - 新增 per-object 运行时状态字段。
- `src/Ext/Techno/Body.cpp`
  - 初始化并序列化运行时状态。
- `src/Ext/Aircraft/Body.h`
  - 声明 Aircraft 自动索敌辅助函数。
- `src/Ext/Aircraft/Body.cpp`
  - 实现停靠判断、扫描、目标过滤和任务派发辅助函数。
- `src/Ext/Aircraft/Hooks.cpp`
  - 在已有 Aircraft update hook 中调用辅助函数。

禁止修改或应避让文件：

- `src/Ext/WeaponType/Body.h`
- `src/Ext/WeaponType/Body.cpp`
- `src/Ext/Techno/Hooks.Firing.cpp`
- PhobosFog 相关本地改动文件。

预算结论：

- Phase 1 最小实现预计修改 8 到 9 个文件，超过默认 5 文件预算。
- 建议拆分为两个实现阶段：
  - Phase 1A：只加 INI 字段和运行时状态，不接行为。
  - Phase 1B：接 Aircraft 自动索敌行为。

## 5. 运行时状态

运行时状态放入 `TechnoExt::ExtData`：

```cpp
int DockedAircraftAutoAttack_LastScanFrame;
bool DockedAircraftAutoAttack_DisabledByDeploy;
```

字段语义：

- `DockedAircraftAutoAttack_LastScanFrame`
  - 最近一次扫描帧。
  - 用于实现 per-object interval，避免所有 aircraft 同帧扫描。
- `DockedAircraftAutoAttack_DisabledByDeploy`
  - 单个 aircraft 的临时禁用状态。
  - Phase 1 只预留和序列化，默认 `false`。

序列化要求：

- 两个字段必须加入 `TechnoExt::ExtData::Serialize`。
- 存档读取后不得重置 `DisabledByDeploy`。
- 如果 Phase 1A 只添加字段但不接行为，也必须保持默认值和序列化顺序稳定。

## 6. 生命周期接入点

推荐接入点：

- 优先使用现有 `src/Ext/Aircraft/Hooks.cpp::AircraftClass_Update_UnlandableDamage`。
- 该 hook 位于 Aircraft update 后段，当前已处理 `AirportBound`、`Airstrike`、`Spawned`、dock 查找与 extended mission 相关逻辑。
- 未来实现只在该已有 hook 的安全分支中调用 `AircraftExt::TryDockedAutoAttack(pThis)`，不新增 hook 地址。

建议调用时机：

- 在确认 `pThis->IsAlive`、`pType->AirportBound`、非 `Airstrike`、非 `Spawned` 之后。
- 在不会打断当前攻击、起飞、降落、返航状态的分支中执行。
- 如果当前对象已经有攻击目标、正在空中或处于 unsafe mission status，直接返回 false。

是否需要新增 hook：

- Phase 1 契约结论：不需要新增 hook。
- 如果实现阶段发现现有 Aircraft update hook 无法安全覆盖停靠状态，则停止并进入 hook recon，不允许猜地址新增 hook。

## 7. 停靠判断

推荐停靠判断函数：

```cpp
bool AircraftExt::IsDockedForAutoAttack(AircraftClass* pThis);
```

判断条件必须全部满足：

- `pThis != nullptr`。
- `pThis->IsAlive`。
- `!pThis->InLimbo`。
- `!pThis->Airstrike`。
- `!pThis->Spawned`。
- `!pThis->Team`。
- `!pThis->IsInAir()`。
- `pThis->HasAnyLink()`。
- `TechnoExt::HasRadioLinkWithDock(pThis)`。
- 关联建筑类型是当前 AircraftType `Dock` 列表中的合法 dock。
- 当前 mission / mission status 不是 Attack、Move、Enter、TakeOff、Land、ReturnToBase 或 strafing 相关不安全状态。
- 未处于 EMP、Temporal、Deactivated 等会阻断正常行动的状态。

辅助参考：

- `TechnoExt::HasRadioLinkWithDock` 用于确认 radio link 指向合法 dock。
- `DockNowHeadingTo` 表示正在前往 dock，不等价于已停靠。
- `HasAvailableDock` 只表示所属方存在可用 dock，不等价于当前 aircraft 已停靠。

## 8. 目标选择

推荐目标选择函数：

```cpp
AbstractClass* AircraftExt::FindDockedAutoAttackTarget(AircraftClass* pThis);
```

最小目标选择流程：

1. 以当前 aircraft 所在 cell 或 linked dock 建筑所在 cell 为中心。
2. 使用 `DockedAircraftAutoAttack.Range` 扫描范围内目标。
3. 只考虑敌对合法目标。
4. 按 `DockedAircraftAutoAttack.WeaponOrder` 依次测试武器 `0 / 1`。
5. 对每个候选目标调用现有武器合法性判断：
   - 优先复用 `AircraftClass::SelectWeapon` 或 `TechnoExt::PickWeaponIndex` 的已有语义。
   - 保留 `WeaponTypeExt::CanTarget*` 过滤效果，但不修改 `WeaponTypeExt`。
   - 不允许自动索敌比手动攻击更宽松。
6. 找到第一个合法目标即可，不做复杂评分。

与 `GreatestThreat` 的关系：

- 可优先评估复用 `pThis->GreatestThreat(...)`，因为现有 wrapper 已排除 `Team / Airstrike / Spawned` 并合并主副武器 `AllowedThreats`。
- 如果 `GreatestThreat` 无法限制自定义 Range 或 WeaponOrder，则 `FindDockedAutoAttackTarget` 应做范围和武器顺序的外层约束。
- 不建议在 Phase 1 重写复杂目标评分。

缺 Ammo 规则：

- Phase 1 使用原生 `pThis->Ammo >= MinAmmo`。
- 不做每武器弹药判断。
- 不调用或设计 `AircraftWeaponAmmo`。

## 9. 任务派发

推荐任务派发函数：

```cpp
bool AircraftExt::TryDockedAutoAttack(AircraftClass* pThis);
```

派发原则：

- 找到目标后设置标准 Aircraft 任务状态：
  - `SetTarget(target)`
  - 必要时 `SetDestination(target, true)`
  - `QueueMission(Mission::Attack, false)`
- 派发后由既有 `AircraftClass_Mission_Attack`、`AircraftClass_SelectWeapon_Wrapper` 和 `AircraftExt::FireWeapon` 处理飞行、开火、扣 Ammo 与返航。

禁止行为：

- 不直接调用 `AircraftExt::FireWeapon`。
- 不停机坪原地开火。
- 不手动扣 Ammo。
- 不手写返航流程。
- 不绕过 Aircraft mission。

## 10. Deploy 临时禁用方案

Phase 1 选择 A：只预留字段，不实现 deploy 切换。

原因：

- Deploy 接入需要额外 hook 或复用其它部署路径，当前阶段没有必要扩大 hook 面。
- 停靠自动索敌最小闭环可先不依赖 deploy 切换。
- `DockedAircraftAutoAttack_DisabledByDeploy` 先作为存档兼容字段保留，Phase 5 再接入行为。

未来语义：

```text
Deploy while safely docked:
    DockedAircraftAutoAttack_DisabledByDeploy = !DockedAircraftAutoAttack_DisabledByDeploy
```

未来限制：

- 只能停靠时切换。
- 空中、攻击、降落、返航时禁止切换。
- 只影响当前 aircraft 对象，不修改 AircraftType。

## 11. Hook recon 清单

Phase 1 契约结论：不新增 hook。

如果未来实现阶段必须新增 hook，必须先完成以下 recon：

- hook address。
- stolen-byte size。
- 原始指令边界。
- return address。
- 进入点寄存器和 stack 语义。
- 是否覆盖相对跳转或 call。
- 是否与既有 Phobos / Ares hook 冲突。
- 是否只影响 Aircraft update，不影响 logic / damage / command / spawn path。
- 是否会改变多人同步。

如果只修改已有 hook body：

- 不改变 `DEFINE_HOOK` 地址和 size。
- 不改变 return path。
- 仍需审查行为风险，尤其是每帧扫描成本和 mission 状态打断。

## 12. 保存 / 读取方案

配置字段：

- `RulesExt` 新字段按现有 `LoadFromINIFile` 和 `Serialize` 模式处理。
- `TechnoTypeExt` 新字段按现有 AircraftType / TechnoType 字段读取模式处理。

运行时字段：

- `TechnoExt::ExtData` 新增字段必须在构造函数初始化。
- 必须加入 `TechnoExt::ExtData::Serialize`。

存档兼容风险：

- Phobos 当前 extension 序列化通常依赖固定字段顺序。
- 新字段应追加在相关 Aircraft runtime 字段附近，并在实现说明中标出 save/load 兼容风险。

## 13. 风险清单

- 多人同步风险：目标扫描必须 deterministic，不得依赖非同步随机数或本地 UI 状态。
- 每帧成本风险：多个 aircraft 同帧扫描可能增加开销，应使用 per-object interval 和 frame 分散。
- mission 打断风险：错误判断停靠状态会打断起飞、降落、返航或 attack mission。
- Airstrike / Spawned / Team aircraft 风险：必须默认排除，不允许自动接管脚本或特殊来源飞机。
- 原生 Ammo 风险：Phase 1 使用单 Ammo，未来双弹仓接入时需要重新定义兼容层。
- WeaponOrder 风险：如果 wrapper 最终重新选择了不同武器，Phase 1 的目标选择结果可能与最终开火武器不同，需要在实现中确认。
- Range 默认值风险：默认回退 `Sight` 会产生隐式行为，因此契约选择 `0`。
- Deploy 预留风险：字段存在但 Phase 1 不接行为，文档必须说明。
- Hook 行为风险：虽然不新增 hook，但修改已有 Aircraft hook body 仍可能影响 Aircraft update。
- 本地改动风险：当前 `WeaponType` 和 PhobosFog 相关文件有未提交改动，Phase 1 不得触碰。

## 14. 验收标准

Contract 阶段验收：

- 明确新增字段、默认值和语义。
- 明确 Phase 1 不做双弹仓、不做 UI、不做 `AircraftWeaponAmmo`。
- 明确修改文件预算和禁止修改文件。
- 明确不新增 hook 的首选方案。
- 明确停靠状态判断。
- 明确目标选择与任务派发策略。
- 明确 `Team / Airstrike / Spawned` 排除。
- 明确保存 / 读取字段方案。
- 明确风险和下一阶段实现指令。

未来实现阶段验收：

- `[General] DockedAircraftAutoAttack=false` 时完全无行为变化。
- 未设置 `DockedAircraftAutoAttack=yes` 的 AircraftType 无行为变化。
- `DockedAircraftAutoAttack.Range=0` 时不扫描。
- 启用且停靠的 aircraft 能在范围内发现合法目标并自动起飞攻击。
- 超出 Range 的目标不会触发。
- Ammo 低于 `MinAmmo` 不触发。
- `Airstrike`、`Spawned`、`Team` aircraft 默认不触发。
- 攻击后仍按原 Aircraft 流程返航和装弹。
- 保存读取后 `DockedAircraftAutoAttack_DisabledByDeploy` 不丢失。
- 不修改 `WeaponType` 文件，不覆盖 PhobosFog 本地改动。
- Debug build 通过。

## 15. 下一阶段实现指令

```text
@do 执行 DockedAircraftAutoAttack Phase 1A：只添加 INI 字段和 TechnoExt 运行时状态，不接自动索敌行为。

硬性限制：
- 不新增 hook。
- 不修改 WeaponTypeExt。
- 不修改 Techno/Hooks.Firing.cpp。
- 不修改 PhobosFog 相关文件。
- 不实现双弹仓。
- 不实现 UI。
- 不实现 AircraftWeaponAmmo。

允许修改：
- src/Ext/Rules/Body.h
- src/Ext/Rules/Body.cpp
- src/Ext/TechnoType/Body.h
- src/Ext/TechnoType/Body.cpp
- src/Ext/Techno/Body.h
- src/Ext/Techno/Body.cpp

实现目标：
- 新增 RulesExt 全局字段：
  - DockedAircraftAutoAttack=false
  - DockedAircraftAutoAttack.Interval=15
- 新增 TechnoTypeExt 字段：
  - DockedAircraftAutoAttack=false
  - DockedAircraftAutoAttack.Range=0
  - DockedAircraftAutoAttack.Interval=-1
  - DockedAircraftAutoAttack.MinAmmo=1
  - DockedAircraftAutoAttack.WeaponOrder=0,1
  - DockedAircraftAutoAttack.DisableOnDeploy=false
- 新增 TechnoExt runtime 字段：
  - DockedAircraftAutoAttack_LastScanFrame
  - DockedAircraftAutoAttack_DisabledByDeploy
- 完成初始化、INI 读取和序列化。
- 不改变任何运行时行为。

验证：
- 运行 scripts\build_debug.bat。
- 输出修改文件、public API 影响、序列化风险和手动验证步骤。
```
