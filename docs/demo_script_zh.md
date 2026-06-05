# 期末報告 Demo 腳本：Rust Concurrency Programming

## Demo 主題

本 demo 用「高併發演唱會售票平台」說明 concurrency 與 parallelism 在軟體工程中的不同角色。

一句話版本：

> C unsafe counter 展示 race condition；Rust mpsc 展示即時訂單服務；Rayon 展示售後批次分析。

## 開場講法

今天的需求不是只寫一個會扣票的程式，而是設計一個小型售票平台。

我們會看三個階段：

1. C 的共享變數造成超賣。
2. Rust 用 `mpsc` 把多個前端訂單送到中央售票服務。
3. 售票結束後，用 Rayon 對大量銷售紀錄做平行分析。

系統必須維持兩個 business invariants：

```text
sold + remaining == initial_inventory
sold + rejected == submitted_orders
```

## Part 1：C unsafe race condition

執行：

```powershell
New-Item -ItemType Directory -Force -Path target
gcc c_demo/ticket_race.c -O2 -pthread -o target/c_ticket_race.exe
.\target\c_ticket_race.exe unsafe 100 16 80
```

講解：

- 多個 thread 同時讀寫 `tickets_left`。
- 程式刻意在讀取與寫回之間加入 delay，讓 race condition 更容易被觀察到。
- 如果輸出出現 `oversold: true` 或 invariant false，就代表系統賣出的票數已經不可信。

接著執行：

```powershell
.\target\c_ticket_race.exe mutex 100 16 80
```

講解：

- `pthread_mutex` 把查票與扣票包成 critical section。
- 這可以修正 race condition。
- 但 C 不會強制每個共享狀態都必須被保護，安全性仰賴工程師紀律。

## Part 2：Rust mpsc 即時訂單服務

執行：

```powershell
cargo run --release -- mpsc --tickets 100 --producers 16 --orders 80 --queue-capacity 32 --service-delay-us 500
```

講解：

- `producers` 代表多個前端節點或售票窗口。
- 每個 producer 把 `Order` 送進 bounded mpsc channel。
- 中央 ticket office 是唯一 consumer，也是唯一擁有庫存的地方。
- 多個 producer 不直接改庫存，因此 race condition 被架構邊界隔離。

錄影時可以指輸出中的幾個欄位：

```text
submitted orders
sold
rejected/sold out
remaining by tier
invariant ...
producer sends delayed by backpressure
```

補充講法：

> 這裡 mpsc 的重點不是速度，而是 live system 中的訊息傳遞與 ownership boundary。bounded channel 還能讓我們觀察 backpressure，也就是中央服務處理不夠快時，producer 會被迫等待。

可以再跑一次小 queue：

```powershell
cargo run --release -- mpsc --tickets 100 --producers 16 --orders 80 --queue-capacity 1 --service-delay-us 500
```

觀察 `producer sends delayed by backpressure` 是否變多。

## Part 3：Rayon 售後批次分析

執行：

```powershell
cargo run --release -- rayon --analytics-records 1000000 --producers 16
```

講解：

- 售票結束後，系統要產生報表。
- 每一筆銷售紀錄大多可以獨立分析。
- Rayon 用 `par_iter`、`fold`、`reduce` 把資料切到多個 CPU core 上處理。

輸出重點：

```text
sequential elapsed
parallel elapsed
speedup
sequential result equals parallel result
revenue
fraud/manual review candidates
busiest source
```

補充講法：

> Rayon 適合 batch analytics，不適合硬拿來做共享庫存扣減。扣庫存有順序與 ownership 問題；銷售紀錄分析則是大量獨立資料處理，這才是 Rayon 的自然場景。

如果 speedup 不明顯，可以加大資料量：

```powershell
cargo run --release -- rayon --analytics-records 2000000 --producers 16
```

## Part 4：完整串接

執行：

```powershell
cargo run --release -- all --tickets 100 --producers 16 --orders 80 --queue-capacity 32 --analytics-records 500000
```

講解：

- Rust CLI 的 `all` 會提醒先看 C counter。
- 接著跑 mpsc 即時訂單服務。
- 最後用 live sale records 作為種子，擴充成大量 historical sales records 交給 Rayon 做 batch analytics。

## 結尾講法

這個 demo 的結論是：

```text
C unsafe shared counter -> 會破壞售票 invariant
Rust mpsc order service -> 用 message passing 與 ownership boundary 管理即時訂單
Rayon batch analytics -> 用資料平行加速大量售後統計
```

所以 `mpsc` 和 Rayon 都是 Rust 並行程式設計的重要工具，但它們應該放在不同的工程問題裡展示。

