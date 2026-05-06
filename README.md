# 📡 インターホン自動解錠システム

不在時のインターホン点灯を照度センサで検知し、SwitchBot Bot が物理的に解錠ボタンを押すシステムです。
Discord への通知機能と、ブラウザから操作できる Web UI を備えています。

今回基本コードはClaudeで書きました。
CADデータは自作です。
ESP等の設定でうまくいかない場合はclaudeに聞くか私のnoteをみてください。

---

## 構成

| デバイス | 役割 |
|---|---|
| ESP32-DevKitC-32E | 制御・Wi-Fi通信 |
| BH1750 (GY-302) | インターホン画面の点灯検知 |
| SwitchBot Bot | 解錠ボタンを物理押下 |
| Discord Webhook | スマホへのプッシュ通知 |

---

## ピン配置（BH1750）

| BH1750ピン | ESP32ピン |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SCL | GPIO22 |
| SDA | GPIO21 |
| ADDR | GND（I2Cアドレス 0x23固定） |

---

## 必要なライブラリ（Arduino IDE）

- `BH1750` by Christopher Laws
- `WiFi`（ESP32標準）
- `WebServer`（ESP32標準）
- `Preferences`（ESP32標準）
- `HTTPClient`（ESP32標準）
- `mbedtls`（ESP32標準）

---

## 3Dプリントパーツ

`cad/` フォルダに固定器具のデータがあります。
対象インターホン：VGDB18543W
推奨素材：PLA

---

## セットアップ

### 1. 認証情報の設定

`secrets.h.example` を `secrets.h` にコピーして、各値を入力してください。

```bash
cp secrets.h.example secrets.h
```

```cpp
// secrets.h
#define WIFI_SSID      "your_ssid"
#define WIFI_PASS      "your_password"
#define SB_TOKEN       "your_switchbot_token"
#define SB_SECRET      "your_switchbot_secret"
#define SB_DEVICE_ID   "your_device_id"
#define DISCORD_WEBHOOK "https://discord.com/api/webhooks/..."
```

> **注意：`secrets.h` は `.gitignore` に含まれています。絶対にコミットしないでください。**

### 2. SwitchBot トークンの取得

1. SwitchBot アプリ → プロフィール → 開発者オプション
2. `トークン` と `シークレットキー` をコピー

### 3. Discord Webhook の取得

1. Discord チャンネル設定 → 連携サービス → ウェブフックを作成
2. URL をコピー

### 4. 書き込み

Arduino IDE でボードを `ESP32 Dev Module` に設定し、書き込んでください。

---

## 使い方

1. ESP32 が起動するとシリアルモニタに IP アドレスが表示されます
2. ブラウザで `http://<IPアドレス>` にアクセス
3. Web UI から以下を操作できます

| 操作 | 説明 |
|---|---|
| 自動解除 ON/OFF | インターホン検知時の自動解錠を切替 |
| 閾値設定 | 検知感度を lux 単位で調整 |
| ベースライン再較正 | 環境光が変化した際に再測定 |

---

## システムフロー

```
BH1750 が点灯検知
    ↓
ESP32 が「来客あり」と判断（閾値超過）
    ↓
自動解除 ON の場合 → SwitchBot を駆動
    ↓
Discord に通知
```

---

## 注意事項

- Web UI に認証はありません。**同一 LAN 内限定**での使用を推奨します
- 冷却時間（クールダウン）は 30 秒に設定されています
- 本システムは光学検知・物理押下による**非破壊・原状回復可能**な設計です

---

## 将来の拡張予定

- [ ] 解錠後の状態フィードバック検知
- [ ] Raspberry Pi による一元管理ダッシュボード
- [ ] Web UI への認証機能追加
