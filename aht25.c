#include "aht25.h"
#include "i2c.h"
#include "lcd.h"
#include <util/delay.h>
#include <avr/io.h> // for _delay_ms

// AHT25を初期化する
bool aht25_init(void) {
    _delay_ms(150); // 電源投入後の安定待ち（メーカーコードの PowerOnTim 相当）
    // 0x71コマンドを送る
//    if (!i2c_start()) { i2c_stop(); return false; }
//    if (!i2c_write(AHT25_ADDRESS << 1 | 0x00)) { i2c_stop(); return false; } // 書き込みアドレス
//                                                                             // データシート7.4.1より、ステータスバイトは0x71コマンドで取得
//    if (!i2c_write(0x71)) { i2c_stop(); return false; } // ステータス読み出しコマンド
//    i2c_stop();
//    _delay_us(75); // データシート記載の待機時間
    return true; // 何も送らずに「成功」としてメインループへ進む！
}

// AHT25のステータスレジスタから、キャリブレーションが行われているか確認する
// この関数はaht25_initで使われるもので、aht25_read_data内でのBusyチェックとは別
bool aht25_is_calibrated(void) {
    uint8_t status;
    // データシート7.4.1より、ステータスバイトは0x71コマンドで取得
    if (!i2c_start()) { i2c_stop(); return false; }
    if (!i2c_write(AHT25_ADDRESS << 1 | 0x00)) { i2c_stop(); return false; } // 書き込みアドレス
    if (!i2c_write(0x71)) { i2c_stop(); return false; } // ステータス読み出しコマンド
    i2c_stop();
    _delay_us(75); // データシート記載の待機時間

    if (!i2c_start()) { i2c_stop(); return false; }
    if (!i2c_write(AHT25_ADDRESS << 1 | 0x01)) { i2c_stop(); return false; } // 読み込みアドレス
    status = i2c_read_ack(); // ステータスレジスタを読み込む
    if (status == 0xFF) { i2c_stop(); return false; } // エラーチェック
    i2c_stop();

    return (status & AHT25_STATUS_CAL_MASK) != 0;
}

// 温度と湿度を読み取る
// temperature: 温度を格納するポインタ
// humidity: 湿度を格納するポインタ
// raw_data: 生の7バイトデータ (status + 6 data) を格納するポインタ
// 成功した場合 true, 失敗した場合 false を返す
bool aht25_read_data(float *temperature, float *humidity, uint8_t *raw_data) {
    uint8_t buf[7];
    for (int i = 0; i < 7; i++) raw_data[i] = 0; // 事前にraw_dataをクリアしておく

    // 1. 測定開始
    if (!i2c_start()) { /*lcd_debug_message("E1");*/ return false; }
    if (!i2c_write(0x38 << 1 | 0)) { i2c_stop(); /*lcd_putstr("E2");*/ return false; }
    i2c_write(0xAC);
    i2c_write(0x33);
    i2c_write(0x00);
    i2c_stop();

    // 2. メーカー推奨の「150ms」待機（ここがポイント！）
    _delay_ms(500); 

    // 3. 読み出しリトライ（粘り強く！）
    bool success = false;
    bool is_started = false; // 測定開始コマンドが送られたかどうかのフラグ
    for (int i = 0; i < 50; i++) {
        buf[1] += 1; // リトライ回数をraw_dataに保存（デバッグ用）
        is_started = false; // 毎回リトライするたびにフラグをリセット
        if (i2c_start()) {
            is_started = true; // 測定開始コマンドは送られている
            if (i2c_write(0x38 << 1 | 1)) {
                success = true;
                break;
            }
            i2c_stop();
        }
        _delay_ms(10);
    }
    if (!is_started) {
      //lcd_debug_message("E3");
      return false;
    }
    if (!success) {
      //lcd_debug_message("E4");
      return false;
    }
    raw_data[0] = 0x38 << 1 | 1; // 読み込みアドレスをraw_dataに保存

    // 4. データ受信
    for (int i = 0; i < 6; i++) buf[i] = i2c_read_ack();
    buf[6] = i2c_read_nack();
    i2c_stop();

    // 生データを呼び出し元にコピー
    for (int i = 0; i < 7; i++) raw_data[i] = buf[i];

    // 5. 公式の判定基準（0x18チェック）
    // busyフラグが落ち、かつ校正済みであることを確認
    if ((buf[0] & 0x88) != 0x08) {
        return false; 
    }

    // 6. 計算 (サンプルコードの s32x の結合と同じロジック)
    uint32_t hum_raw = ((uint32_t)buf[1] << 12) | ((uint32_t)buf[2] << 4) | (buf[3] >> 4);
    uint32_t tem_raw = (((uint32_t)buf[3] & 0x0F) << 16) | ((uint32_t)buf[4] << 8) | buf[5];

    *humidity = (float)hum_raw * 100.0 / 1048576.0;
    *temperature = (float)tem_raw * 200.0 / 1048576.0 - 50.0;

    return true;
}
