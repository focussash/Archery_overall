# 射箭视频mocap + M5Stick 训练平台 — 草稿 v1

v0：2026-11-14，web 端 Claude 生成（原文备份在 `射箭视频mocap + M5Stick 训练平台 — 草稿 v0 (web).md`）
v1：2026-11-16，Claude Code 对照另外两个项目的现状修订 · @Zijie

执行层面的文件：`CLAUDE.md`（工作规则）、`claude-memory/DEVELOPMENT_PLAN.md`（具体步骤）、`claude-memory/architecture.md`（模块和接口）、`claude-memory/DECISIONS.md`（已定和待定）。这份 brief 只管"为什么"。

## 一句话结论

这份文件是交给 Claude Code 的大方向 brief。先搭一个能跑、能看的粗糙原型，再一步步 refine，必要时 refactor。路线是：**一台侧面相机（拍到的是冠状面 frontal plane）+ 手机本地跑姿态估计 (on-device pose estimation)**，和已有的 M5Stick 可穿戴、ArcheryApp 记分 app 配合。视频负责 IMU 看不到的东西（拉弓手轨迹、收势、头肩姿态、时序），IMU 负责视频看不清的东西（cant、撒放瞬间）。实时反馈在本地完成，箭后复盘和重分析可以走云。

有四条硬约束，Claude Code 从第一天就得当真：

1. **实时反馈只能在本地跑（手机或身上的 M5Stick），不能走云。** 满弓 (full draw) 到撒放 (release) 只有一两秒，上传、云端推理、再传回来，延迟和抖动都不可控。所以是两条链路：实时链路 (real-time path) 全在本地，离线链路 (offline path) 可以上云。
2. **非丁非八是开放式站姿 (open stance)，躯干不平行于相机成像平面。** 拉距、肩宽这些 2D 量会被透视缩短 (foreshortening)。只跟自己比的话问题不大，前提是每次相机位置和站位一致：要有一套摆放流程 (camera placement protocol)，指标尽量用比值或角度，少用绝对长度。将来给别人用，这条更重要。
3. **靠位 (anchor) 不能当成切阶段的必要条件。** 空拉过耳的人没有触脸靠位，所以满弓或持弓阶段 (full draw / hold) 用拉弓手（或拉弓臂）速度降到阈值以下来定义。注意：如果持弓时持续发力、拉弓手还在缓慢后走，速度不会真正降到 0，要用相对阈值或速度的局部极小值。靠位相关的指标做成可选模块。
4. **同步提示的语义是"已满弓、继续维持"，不是撒放信号。** 这跟竞技反曲的响片 (clicker)"响了就撒"正好相反。设计和数据记录都要能检测提示有没有被条件化成响片（见"同步提示"一节）。

## 已定的事（2026-11-16 更新）

| 项 | 已定 | 对设计的影响 |
| --- | --- | --- |
| 相机 | 1 台，放侧面三脚架（射线旁、正对胸前），拍冠状面 | 第二台相机写进路线图，但不进第一版 |
| 代码放哪 | Python 研究代码放本文件夹；Android 部分做成 ArcheryApp 里的独立 Gradle 模块，取代 ArcheryApp 原来的 Phase 4 | 理由见"跟现有两个项目的关系" |
| 姿态模型 | 起步用 MediaPipe Pose Landmarker，Python 和 Android 用同一个 `.task` 模型文件；RTMPose / RTMW 做离线对照和箭后重分析 | 模型藏在接口后面；MediaPipe 不达标就两端一起换 RTMPose（ONNX） |
| 反馈时机 | 两种都要：拉弓中的同步反馈 (concurrent feedback，满弓提示)；箭后几秒的终末反馈 (terminal feedback)，以收势 follow-through 为主 | 两套不同的规则和延迟预算 |
| 反馈通道 | 不用手机振动（手机在三脚架上，离人两三米）。占位：手机外放提示音、M5Stick 蜂鸣。将来：骨传导耳机或别的 wearable | 输出接口可插拔；每种通道的延迟要实测并记录 |
| 指标 | physics-informed：只收有生物力学或教练依据的指标，在这个前提下越多越好；收势优先；靠位可选 | 指标注册制 (metric registry)；每个指标附机理说明和噪声底 |
| 撒放事件来源 | 可插拔：视频音轨或手机麦克风里的弦声、视频运动学、M5Stick IMU | 离线标签用音轨；实时触发可以三选一或融合 |
| 站姿 | 开放式，高颖"非丁非八" | 见硬约束 2 |
| 用户 | 先自己用，将来给别人用；同一时间只拍一个人 | 不做多人检测；个人参数（身高、左右手、有无靠位）做成配置文件 (profile) |
| M5Stick | 已经能用（见下一节），BLE 协议以 M5Stick 项目的 `overall_design.md` 为准 | 本项目不改固件；需要固件配合的改动列成需求清单，到那边按那边的流程做 |

## 跟现有两个项目的关系

### M5Stick（`../wearable m5stick`，做到 SP13）

已经有的：

- 持弓手 (primary) 和拉弓臂 (secondary) 两个 IMU，ESP-NOW 50 Hz 互传。
- 射箭状态机 `IDLE → DRAW_DETECTED → HOLD → SHOT_FIRED → RECOVERY`。HOLD 用角速度低于阈值判定，撒放用加速度尖峰判定。
- **已经有同步提示**：进入开弓时一个短 blip（`DRAW_NOTIFY`），进入 HOLD 且姿态在容差内时双 blip（`FORM_ALIGNED`），持弓中偏离容差时低 blip（`FORM_DEVIATED`）。这基本就是"已满弓"提示，只是判据来自 IMU。
- 箭后：节奏判断（`RUSHING` / `HOLDING_TOO_LONG`），以及 cant、仰角、拉弓角的终末蜂鸣。
- BLE GATT：每箭一个 20 字节的 `ShotPacket_t`（时间戳、cant、仰角、拉弓角、角速度、箭号、form flags），64 箭环形缓冲；手机能发的命令只有 session start / stop。

没有的：

- 振动马达。只有蜂鸣器；振动是 Grove 口外接的 future 项。
- 由手机触发"播放某个 pattern"的 BLE 命令。
- 每箭的开弓、持弓时长不在 `ShotPacket_t` 里（固件内部的 `ShotTiming` 有）。

给 M5Stick 项目的需求清单（这里只写桩，不碰那边的代码）：

1. BLE 命令：播放指定 pattern。这样手机算出的反馈能从 M5Stick 出声。
2. 把 `hold_ms`、`draw_ms` 塞进 `ShotPacket_t.reserved`（4 字节，正好两个 uint16），本箭有没有发同步提示放进 `form_flags` 的空余位。这是监测"提示变响片"的关键数据。
3. （可选）同步提示可开关，并能按 fading schedule 随机省略。
4. （可选）录 M0 数据时能连续导出 IMU 原始流，方便和视频对齐。

### ArcheryApp（`../Archery_android_app`，做到 Phase 2.4.7）

- Phase 3（BLE 接 M5Stick）还没开始。Phase 4 原计划是 ML Kit 单张照片的静态姿态分析，被这份方案取代。
- **决定：放进 ArcheryApp，但做成独立 Gradle 模块**（例如 `:feature:mocap`，再加一个共享的 `:core:ble`）。记分部分不依赖它，不用的时候不加载，运行时不会变重；真正重的分析本来就在笔记本或云上。放在一起的理由：
  - 手机在三脚架上不动，两组之间直接在同一个 app 里录分，不用切 app（Android 切到后台会丢掉相机）。
  - M5Stick 被一台手机连上后就停止广播，同一时间只有一个 app 能连，两个 app 会抢。
  - 每箭的动作记录和环数在同一个库里，自动关联。
- 修改 ArcheryApp 的 `DEVELOPMENT_PLAN.md`（重写 Phase 4）要按它的 CLAUDE.md 先征得你同意，到 M3 开始前再做。

## 架构草图 (architecture)

```
【实时链路 · 本地】
相机 → 姿态估计（可替换）→ 因果滤波 → 阶段识别 → 同步提示规则 → 反馈输出
M5Stick IMU 状态机（DRAW / HOLD）→ 同步提示（M5Stick 自己已经在做）

撒放触发源（可插拔）：麦克风弦声 / 视频运动学 / M5Stick 撒放事件（BLE）
          ↓
帧（缩到模型输入尺寸）或关键点的环形缓冲 → 撒放后取前后窗口 → 非因果分析
          → 终末指标 → 终末反馈规则 → 反馈输出

反馈输出（可插拔）：手机外放 / M5Stick 蜂鸣（需固件加命令）/ 骨传导耳机 / …

【箭后链路 · 可以慢】
每箭记录（关键点、指标、提示开关与送达时刻、箭号与环数）
  → ArcheryApp 本地库 → 上传 → 重模型重分析（RTMW 等）→ 笔记本或网页复盘
```

每个框在代码里是一个可以单独替换的模块：视频输入、姿态估计、滤波、阶段识别、指标、反馈规则、反馈输出、撒放触发、存储、报告。比如姿态模型从 MediaPipe 换成 RTMPose，不该影响阶段识别和指标。

## 先纠一个概念：相机放哪，看到的是哪个平面

射箭时你是侧对靶子站的，箭走的是你两肩连线的方向（内外侧, mediolateral）。所以开弓 (draw)、靠位 (anchor)、撒放 (release) 这些动作，主要发生在你自己的**冠状面 (frontal / coronal plane) + 横断面 (transverse plane)**，而不是矢状面 (sagittal plane)。相机站在射线旁边、正对你胸口，在解剖学上拍到的是**冠状面**。Sports2D 写明了动作只要在矢状面或冠状面里都行，关键是要确认你想看的东西在不在这个平面里。

| 相机位置 | 拍到的平面 | 看得清 | 看不清 | 安全 |
| --- | --- | --- | --- | --- |
| 射线旁、正对你胸前 | 冠状面 | 持弓臂 (bow arm) 高度、两肩高低差 (shoulder height difference)、头的倾斜 (head tilt)、拉距 (draw length，这里看水平位移)、靠位时手离嘴的距离、撒放后手往哪走 (follow-through) | 拉弓肘 (draw elbow) 在不在箭线后面、躯干转动 (trunk rotation)、持弓肩有没有前顶 (bow-shoulder protraction / elevation)、弓的左右倾斜 (cant)（这些都是深度） | 安全 |
| 头顶往下拍 | 横断面 | line / alignment（肘、肩、箭是不是一条线）、躯干转动 | 高度信息 | 安全，但不好架 |
| 你身后、顺着箭的方向拍 | 接近矢状面 | 弓的倾斜（cant），拉弓肘相对箭线的前后、高低（也能看 line，而且比头顶好架） | 拉距（这时变成深度） | 安全 |
| 从靶子那边往回拍 | 矢状面 | — | — | **绝对不行，在射线上** |

视频和 IMU 正好互补：正面相机看不清 cant，M5Stick 直接测 cant；M5Stick 测不到拉弓手在平面内的轨迹和收势，正面相机看得清。指标设计上不要两边重复测同一个量，重叠的量（比如持弓臂仰角、持弓时长）只用来互相校验。

有两件汉弓特有的事情要先有心理准备：

- 你用扳指 (thumb ring)、拇指勾弦 (thumb draw)。现成的手部关键点 (hand keypoints) 模型基本是拿普通手势训练的，拇指被扳指挡住、又贴在脸旁边的时候准不准，得实测才知道。
- 你的靠位是羽毛贴嘴唇，用脸部关键点里的嘴角 (mouth corner) 去定义"靠位是否一致"，在正面相机里是看得到的（MediaPipe Pose 自带左右嘴角点）。

## 市面上已有的东西

商业产品基本都是"弓上或者手腕上一个 IMU + 手机 app"，核心功能就是把一箭切成几个阶段、各自计时、再做箭与箭之间的对比。视频这边有一个刚出来的多相机 app，但不支持传统弓。

| 东西 | 形态 | 测什么 | 对你的参考价值 |
| --- | --- | --- | --- |
| [Mantis X8](https://www.americanhunter.org/content/first-look-mantis-x8-archery-trainer) | 装在弓上的 IMU，蓝牙连 app | 整箭的弓体轨迹，按 USA Archery NTS 分阶段着色、计时；还有 cant（弓的左右倾斜）和稳定分数 (stability score) | 阶段划分 + 计时 + 每箭对比，可以当 M5Stick 那边的功能参照。汉弓没地方装 |
| [Steady-Aim A1](https://lancasterarchery.com/products/steady-aim-a1-shooting-analysis-system) | 装在瞄具杆上 | steadiness/drift 分数、hold time、振动分析 | 同上，需要瞄具 |
| [Garmin ArcheryX](https://the5krunner.com/2026/08/25/garmin-archeryx-explained/) | Fenix 9 手表（加速度计 + 陀螺仪） | 出箭前的持弓臂晃动、MOA 稳定度、瞄准轨迹、执行时间、计箭数 | **最接近你的路线**：传感器戴身上，不装弓上。但官方只支持复合弓，出箭怎么检测也没公开 |
| OnForm 等视频 app | 手机录像 + 画线 | 人工看 | 就是你要自动化掉的那一步（[Bow International](https://bow-international.com/?p=7127) 提到过） |
| [ShotLab + BowPose](https://huggingface.co/jotux/BowPose) | 多相机射箭姿态分析 app，外加一个弓的关键点模型 | 人体姿态 (pose) + 弓（弓把 riser、弓梢 limb tips、稳定杆 stabilizer） | 2026 年 11 月刚发的，CC-BY-4.0，完全用合成图训练，只支持反曲、光弓和复合弓，真实视频上的精度还没测过。汉弓大概率要自己重新训练，但他们"用 Blender 合成数据训练"的思路可以直接照搬 |

学术界有两篇可以借鉴：

- [Lee et al. 2024](https://mdpi.com/2076-3417/14/24/11838)：用正面的转播画面、MediaPipe。靠位开始的判据是"右臂角度 10 帧内平均变化 < 1°"；出箭时刻靠在脸部框里用 Hough 变换找弓弦。最后跟 Dartfish 专家标注比，射箭时长 (shot time，靠位到撒放) MAE 0.34 s（30 fps）。注意这个数字是转播画面上、整段时长的误差，不能直接说明固定机位下视频定不准撒放：60 fps 固定机位、看拉弓手速度尖峰，离线大概率能到 1–2 帧，M1 会实测。撒放主要交给 IMU 或弦声，真正的理由是实时、便宜、而且已经有了。
- [Park & Choi 2026](https://reference-global.com/article/10.2478/ijcss-2025-0016)：用 4 名韩国国家级选手的 17 关节序列训练序列网络。你只跟自己比，这种跨人对比的东西用不上，但它的阶段切分可以参考。

## 开源方案对比

OpenCap Monocular **并不是只能云端跑**，代码是开源的（`github.com/utahmobl/opencap-monocular`）。但它依赖 WHAM 加上一步优化，本地跑等于需要 GPU 做离线处理，实时是不可能的；而且它只在走路、深蹲、坐站这些动作上验证过。只看平面的话，它属于杀鸡用牛刀。

| 方案 | 相机数 | 本地 / 云 | 实时 | 输出 | 判断 |
| --- | --- | --- | --- | --- | --- |
| [MediaPipe Pose Landmarker](https://ai.google.dev/edge/mediapipe/solutions/vision/pose_landmarker) | 1 | 本地 | CPU/GPU 实时 | 33 个点（含左右嘴角、拇指和食指的粗略点） | **起步首选**：Python 和 Android 用同一个 `.task` 模型文件，golden fixtures 两端才对得上。Lee 2024 就是用它做的。要确认 VIDEO / LIVE_STREAM 模式下有没有内置的时间平滑；有的话两端设置保持一致，滤波尽量由我们自己做 |
| [rtmlib](https://pypi.org/project/rtmlib)（RTMPose / RTMW） | 任意 | 本地，ONNX，不依赖 mmcv | RTMPose-m 在 i7 CPU 上 90+ FPS（[论文](https://ar5iv.labs.arxiv.org/html/2303.07399)） | 17/26 个身体点；RTMW 有 133 个全身点 (whole-body keypoints)，含手和脸 | 离线对照和箭后重分析用 RTMW；MediaPipe 不达标时两端一起换 RTMPose |
| [Sports2D](https://www.github.com/davidpagnon/Sports2D) | 1 | 本地，pip 装 | 支持摄像头实时 | 2D 关节角 (joint angles)、节段角 (segment angles)；可按身高或相机距离做透视补偿 (perspective correction) | M0 当天先拿它跑一遍录像，零代码看效果。不当底座：底层是 RTMPose，跟手机端模型不一致 |
| [Pose2Sim](https://github.com/perfanalytics/pose2sim) | ≥2 | 本地 | 离线 | 3D 关键点 + OpenSim 关节角 | **第二阶段**：加一台身后或头顶相机看 line 时用。作者与 Sports2D 是同一人，数据格式通用 |
| [FreeMoCap](https://github.com/freemocap/freemocap) | ≥2（建议 3+） | 本地 | 主要离线 | 3D 骨架，可导出 Blender | Pose2Sim 的备选。标定 (camera calibration) 用 ChArUco + Anipose |
| [OpenCap Monocular](https://mobl.mech.utah.edu/opencap-monocular-released/) | 1 台 iPhone | 默认走云，代码开源 | 否 | 3D 运动学 (kinematics) + 动力学 (kinetics)；转角误差 (rotational error) 4.8°（跟标记点 mocap 比） | 不走主线：上肢精细动作 (fine upper-limb motion) 没验证过，也做不了实时 |

## 最大的技术风险：噪声底 (noise floor)

只跟自己比，系统偏差 (bias) 会抵消，成败取决于姿态估计的抖动 (jitter) 和箭与箭之间真实动作差异的比值。粗算一下：

- 横屏 1080p 装下全身，大约 1.5–2 mm/px；现成模型对静止目标的关键点抖动一般有几个像素。
- 角度类：持弓臂（肩到腕）约 300–400 px，几个像素的抖动约 0.5° 量级，满弓段取平均后可用。
- 满弓期间拉弓手的小位移（creep / expansion，几毫米）：很勉强。
- 收势轨迹（几厘米到十几厘米）：很稳。

这个不试不知道，所以放进 M1 当第一批产出：每个指标算一个信噪比（箭间 SD ÷ 箭内抖动），低于阈值的指标在报告里标"不可用"，不进反馈规则。

可选：如果能用实验室，做一次 marker mocap + 手机同步录像，一次回答三件事：绝对精度、开放站姿下透视缩短有多大、戴扳指时拇指关键点准不准。

## 指标 (metric registry)

原则是 physics-informed，不做数据挖掘式的相关性。每个指标注册时写明：

- 定义和计算方式：用哪些关键点、哪个阶段窗口
- 来源：视频、IMU，还是两者互校；在哪个平面里
- 机理依据：为什么它会影响出箭或一致性（引用或教练经验）
- 单位：优先角度和比值，少用绝对长度
- 噪声底：M1 测出来后回填

第一批候选（冠状面视频）：

| 指标 | 阶段 | 依据 |
| --- | --- | --- |
| 拉弓手撒放后的位移方向和幅度 | 收势 | 撒放干不干净、是否沿箭线方向往后走 |
| 持弓臂撒放后的掉或抬（手腕垂直位移、肩–腕角度变化） | 收势 | 推弓有没有持续到撒放之后 |
| 持弓期拉弓手的水平位移（往前 creep 还是往后 expansion） | 持弓 | 持续发力还是塌；creep 会让拉距变短、出箭能量变小 |
| 开弓时长、持弓时长 | 时序 | 节奏一致性；可以和 M5Stick 的 `ShotTiming` 互校 |
| 撒放时持弓臂仰角 | 撒放 | 和 M5Stick 的 pitch 互校 |
| 两肩高度差、头部倾斜 | 持弓 | 姿态一致性 |
| 嘴角到拉弓手的距离（可选，靠位模块） | 持弓 | 靠位一致性 |
| 提示到撒放的间隔 (cue-to-release interval) | 持弓 | 监测同步提示是否被条件化（见"同步提示"一节） |

## 给 Claude Code 的里程碑 (milestones)

**开发策略：先在笔记本上用 Python 做参考实现 (reference implementation)，再移植到 Android。** 理由是用录好的视频在笔记本上迭代，比每次改完都去靶场实测快得多，而且 Claude Code 能自己跑测试。具体做法：

- 阶段识别和指标写成与平台无关的纯函数，输入是关键点时间序列 (keypoint time series)，不碰视频和 UI。
- golden fixtures 分两层：(1) 关键点序列 → 阶段和指标，两端必须一致，这是主要验收标准；(2) 视频 → 关键点，同一个模型下两端误差在容差以内。
- 姿态模型可替换。MediaPipe 起步；RTMPose-s 论文报的是骁龙 865 上 70+ FPS（[RTMPose 原仓库](https://github.com/open-mmlab/mmpose/tree/main/projects/rtmpose)），你的手机实测多少以实测为准。

1. **M0 录数据（你做）**
   - 相机：横屏，1080p，能开 120 fps 就开（离线用，下采样就能模拟 60 / 30 fps），不行就 60 fps。用 Pro 视频模式锁定对焦、曝光和白平衡；快门尽量 1/500 或更快，减少撒放瞬间的运动模糊。**保留声音**，弦声就是撒放标签。
   - 摆放流程：三脚架脚位和你自己的脚位都在地上贴标记，固定距离和高度。每次开录先拍一张标定物（已知长度的东西或 ChArUco 板），再在画面里拍一下手，用来对齐音画偏差。
   - 箭编号、按顺序射；记分时按箭号录，才能把落点对到具体哪一箭。
   - 戴着 M5Stick、开着 session 录。如果能记下 BLE 包（比如另一台手机开 nRF Connect 记录），撒放时刻就多一个独立来源；记不了也没关系，音轨够用。
   - 录 30–50 箭，其中夹几箭在安全范围内故意做的已知扰动（比如故意 creep、故意掉持弓臂、故意不收势），用来检验指标能不能把它们分辨出来。
2. **M1 离线骨架（Python，笔记本）**
   - 第 0 步：拿 Sports2D 跑一遍 M0 录像，先看效果。
   - 视频 → 关键点（MediaPipe 原始输出）→ 滤波 → 阶段识别 → 指标 → 每箭记录 (JSON) → HTML 复盘报告。
   - 撒放标签用音轨弦声的起始点 (onset)；同时用拉弓手速度尖峰从视频里估撒放，报告两者差多少。
   - 噪声底：每个指标的信噪比表；扰动箭能不能被识别出来。
   - 对照：同一批视频跑 RTMW，比较关键点和指标。
   - 验收：30 箭全部跑通；手标 5 箭的事件时刻，误差小于 2 帧；信噪比表和扰动检验结果写进报告。
3. **M2 模拟实时（还是 Python）**
   - 同一条流程改成逐帧的因果处理 (causal, streaming)，不许看未来帧。重点是满弓起点的因果检测和它的延迟。
   - 如果 M0 有 M5Stick 数据：比较视频和 IMU 判定的满弓起点、撒放时刻，顺便验证 M5Stick 的检测器，并用多次撒放事件估算两边的时钟偏移 (time sync)。
   - 基线：在没有提示的录像上算"满弓到撒放"的间隔分布，作为以后判断条件化的对照。
   - 验收：流式结果和 M1 离线结果一致；满弓检测延迟实测，据此定延迟预算。
4. **M3 Android 原型（ArcheryApp 的 mocap 模块，取代原 Phase 4）**
   - **M3a 终末反馈闭环（先做）**：相机预览 + 本地姿态估计 + 帧或关键点的环形缓冲；撒放触发先用手机麦克风的弦声；撒放后对前后窗口做非因果分析，几秒内给终末反馈（手机外放）；每箭记录关联 ArcheryApp 的 Session / Arrow。先做终末的理由：不需要因果处理，撒放后处理一个几秒的窗口即可，难度低一个量级。
   - **M3b 视频同步提示**：因果满弓检测 → 提示；fading schedule；记录每箭提示开关和送达时刻。只有当视频能提供 IMU 给不了、信噪比又够的持弓期信息时（比如 creep）才有必要做；否则同步提示继续由 M5Stick 负责。
   - 验收：回放测试视频时输出与 Python 参考一致；实拍 30 箭不漏箭、不重复计箭；各反馈通道的延迟实测。
5. **M4 箭后链路**
   - 每箭记录上传到云 → 用更重的模型（RTMW 等）重分析 → 笔记本或网页上看复盘。云服务具体选哪家，到时再定。
6. **M5 接 M5Stick（依赖 ArcheryApp Phase 3 的 BLE 客户端）**
   - M5Stick 的撒放事件作为触发源之一；`ShotPacket_t` 存进每箭记录；用多次撒放事件估时钟偏移；手机算出的反馈从 M5Stick 蜂鸣出声（需要需求清单第 1 条）。
7. **M6（可选）第二台相机**
   - 加身后或头顶的相机，看 line 和 cant。

## 同步提示：满弓提示，不是响片

你的意图：提示告诉你"已满弓"，提示之后继续维持、继续发力，然后再撒放。这跟竞技反曲的响片"响了就撒"正好相反。M5Stick 现在的 `FORM_ALIGNED` 双 blip 已经是这种提示。

两个风险：

- **条件化 (classical conditioning)**：提示反复出现在撒放之前，久了可能变成撒放的 go 信号，也就是变成响片。撒放是已经准备好的动作，附加刺激会缩短反应时 (intersensory facilitation)。
- **依赖 (guidance hypothesis)**：练习时有提示表现好，提示一撤，保持测试 (retention test) 里反而变差（[Sigrist et al. 2013 综述](https://www.doi.org/10.3758/S13423-012-0333-8)；[2025 年的实验](https://www.frontiersin.org/journals/psychology/articles/10.3389/fpsyg.2025.1638827/full)）。降低同步反馈的频率可以减少这种依赖，一箭给同步加终末反馈、下一箭不给的交替方案效果更好（[Park et al. 2000](https://informahealthcare.com/doi/abs/10.1080/00222890009601379)）。

设计上的结论：

- 提示频率和逐步撤除的方式 (fading schedule) 要做成可配置项，包括随机省略提示的 catch trial，不要写死成"每箭都响"。
- 每箭记录：提示开或关、提示送达时刻、撒放时刻。
- 监测条件化：看"提示到撒放"的间隔分布。如果它往简单反应时（约 150–250 ms）收缩、变异也变小，或者有提示的箭持弓明显比没提示的短，说明提示正在变成响片，报告里报警。M2 先用没有提示的录像建立基线。
- 通道延迟：蓝牙耳机（骨传导耳机一样走 A2DP）通常 150–300 ms，BLE 到 M5Stick 是几十 ms，M5Stick 自己判、自己响几乎没有传输延迟。对"已满弓"这种语义，延迟可以容忍，但必须实测并记录，因为"提示到撒放"的间隔要用实际送达时刻来算。

## 还没定的事（故意留到后面）

这些不影响第一版骨架，但代码里要给它们留好位置。

- **数据标签和训练**：每箭的结果 (outcome) 来自 ArcheryApp 按箭号录的环数和落点，外加可选的自评分。要不要训练模型，等有了数据再谈。
- **反馈硬件**：骨传导耳机、别的 wearable、M5Stick 外接 Grove 振动模块，都只是候选。代码里反馈是可插拔的输出接口。
- **视频同步提示要不要做（M3b）**：等 M1 看视频能不能给出 IMU 给不了、而且信噪比够的持弓期指标。
- **云服务选型**：到 M4 再定。
- **给别人用时的个人配置 (profile)**：身高、左右手、有没有靠位、站姿。

## 给 Claude Code 的工作约定

- **先能跑，再好看**：每个里程碑先交一个端到端能跑通的粗糙版，再 refine。可以 refactor，但 refactor 前后都要过 golden fixtures 测试。
- **模块边界按架构草图来**：视频输入、姿态估计、滤波、阶段识别、指标、反馈规则、反馈输出、撒放触发、存储、报告。换掉其中一块不该动到别的。
- **该我拍板的事不要自己定死**：比如指标阈值、反馈规则、提示的 fading schedule、UI 取舍。记到 DECISIONS.md 里，列出选项等我定。纯技术选型（比如具体用哪个模型、哪个库）可以自己定，记下理由。
- **不碰另外两个项目的代码**：M5Stick 只按需求清单写桩；ArcheryApp 在 M3 之前不动，改它的计划先问我。两边各自的 CLAUDE.md 规则照常适用。
- **Python 环境**：用我现有的 conda 环境；装任何包之前先问（跟另外两个项目同一条规则）。
- **为变化而设计 (design for change)**：三个项目的计划都会随进展而变。能换的组件放在接口后面，新功能靠注册加模块而不是改流程，跨边界的数据和协议都带版本号，阈值全进配置文件；但不做没人要的功能。细则见 `claude-memory/architecture.md` 第 6 节，另外两个项目的 CLAUDE.md 里也加了同样的原则。
- **开工前**：~~把这份 brief 转成跟另外两个项目一样的结构~~ 已完成（2026-11-16）。
- **每个里程碑收尾**：更新 README，附一个能直接跑的 demo 命令。

## 参考链接

- [Sports2D (GitHub)](https://www.github.com/davidpagnon/Sports2D) · [JOSS 论文](https://www.theoj.org/joss-papers/joss.06849/10.21105.joss.06849.pdf)
- [MediaPipe Pose Landmarker](https://ai.google.dev/edge/mediapipe/solutions/vision/pose_landmarker)
- [Pose2Sim (GitHub)](https://github.com/perfanalytics/pose2sim) · [PyPI](https://pypi.org/project/pose2sim/)
- [rtmlib](https://pypi.org/project/rtmlib) · [RTMPose 论文](https://ar5iv.labs.arxiv.org/html/2303.07399) · [RTMW 论文](https://arxiv.org/abs/2407.08634) · [RTMPose 原仓库](https://github.com/open-mmlab/mmpose/tree/main/projects/rtmpose)
- [FreeMoCap](https://github.com/freemocap/freemocap)
- [OpenCap Monocular 发布说明](https://mobl.mech.utah.edu/opencap-monocular-released/) · [arXiv 2603.24733](https://arxiv.org/abs/2603.24733) · [CVBW 2026 版](https://cvbw2026.github.io/assets/papers/15.pdf)
- [BowPose / ShotLab](https://huggingface.co/jotux/BowPose)
- [Mantis X8 介绍](https://www.americanhunter.org/content/first-look-mantis-x8-archery-trainer) · [Bow International 测评](https://www.bow-international.com/?p=4652)
- [Steady-Aim A1](https://lancasterarchery.com/products/steady-aim-a1-shooting-analysis-system)
- [Garmin ArcheryX](https://the5krunner.com/2026/08/25/garmin-archeryx-explained/)
- [Lee et al. 2024, Applied Sciences: 靠位到撒放的时长分析](https://mdpi.com/2076-3417/14/24/11838)
- [Park & Choi 2026, IJCSS: 射箭动作序列网络](https://reference-global.com/article/10.2478/ijcss-2025-0016)
- [Sigrist et al. 2013: 运动学习中的增强反馈综述](https://www.doi.org/10.3758/S13423-012-0333-8) · [2025 同步反馈实验](https://www.frontiersin.org/journals/psychology/articles/10.3389/fpsyg.2025.1638827/full) · [Park et al. 2000](https://informahealthcare.com/doi/abs/10.1080/00222890009601379)
