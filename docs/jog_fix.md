# Continuous Jog 修复说明

## 问题

按下 Jog 按钮后松开，机器继续移动不停。

## 原因分析

通过 git diff 发现 MainWindow.cpp 中 jog 连接被修改：

**错误的实现**：
```cpp
// 错误1: 手动拼接字符串，格式不正确
connect(m_jogPanel, &JogPanel::jogContinuousRequested,
        this, [this](...) {
            QString cmd = QString("$J=G91 %1%2 F%3").arg(axis).arg(sign).arg(feed);
            m_core->serialManager()->sendCommand(cmd);
        });

// 错误2: jog cancel 用了 sendCommand，会添加 \r\n
connect(m_jogPanel, &JogPanel::jogCancelRequested,
        this, [this]() {
            m_core->serialManager()->sendCommand("\x85");  // 错误！
        });
```

**问题**：
1. `sendCommand("\x85")` 会把 0x85 加上 `\r\n` 发送，变成 3 字节
2. GRBL realtime 命令必须是单个 raw byte，不能有任何终止符

## 正确实现（参考 GRBL-Plotter Rust 代码）

```rust
// GRBL-Plotter: manager.rs
pub async fn jog_continuous(&mut self, axis: &str, direction: i32, feed: u32) {
    let sign = if direction >= 0 { "" } else { "-" };
    self.send_command(&format!("$J=G91 {}{}9999 F{}", axis, sign, feed)).await
}

pub async fn jog_stop(&mut self) {
    self.send_realtime(0x85).await  // 单字节，无终止符
}
```

## 解决方案

恢复使用 SerialManager 的专用方法：

```cpp
// MainWindow.cpp - 正确的连接方式
connect(m_jogPanel, &JogPanel::jogContinuousRequested,
        this, [this](const QString &axis, int direction, double feed) {
            m_core->serialManager()->jogContinuous(axis, direction, feed);
        });

connect(m_jogPanel, &JogPanel::jogCancelRequested,
        this, [this]() {
            m_core->serialManager()->jogCancel();
        });
```

SerialManager.cpp 中正确实现：

```cpp
void SerialManager::jogContinuous(const QString &axis, int direction, double feedRate)
{
    double dist = (direction >= 0) ? 9999.0 : -9999.0;
    sendCommand(QString("$J=G91 %1%2 F%3").arg(axis).arg(dist, 0, 'f', 0).arg(feedRate));
}

void SerialManager::jogCancel()
{
    sendRealtimeCommand(GrblRealtime::JogCancel);  // 0x85，单字节
}

void SerialManager::sendRealtimeCommand(char command)
{
    m_port->write(&command, 1);  // 只写1字节，不加 \r\n
    m_port->flush();
}
```

## 关键点

| 操作 | 命令格式 | 说明 |
|------|----------|------|
| Continuous Jog | `$J=G91 X9999 F1000` | 大距离值，持续移动直到取消 |
| Jog Cancel | 0x85 (单字节) | realtime 命令，不能加 `\r\n` |

## 附加修复

Console 面板过滤 GRBL 状态报告，避免显示垃圾信息：

```cpp
connect(m_core, &ApplicationCore::responseReceived,
        this, [this](const QString &response) {
            // 过滤状态报告: <Idle|WPos:...>
            if (response.startsWith('<') && response.endsWith('>'))
                return;
            ui->consolePanel->appendMessage(response);
        });
```

## 日期

2026-04-15