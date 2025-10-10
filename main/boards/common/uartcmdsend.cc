#include "uartcmdsend.h"
#include "esp_log.h"
#include "esp_system.h"
#include <functional>
#include <cstring> // 用于memmove等函数
#include <string>
#include "application.h"
#include "assets/lang_config.h"
#include "ssid_manager.h"
// using namespace MyNamespace; // 根据需要取消注释
// 示例代码
// app.PlaySound(MyNamespace::Lang::Sounds::P3_CHARGING);
#include "wifi_station.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "display.h"

#include "freertos/event_groups.h"

// WiFi事件组
static EventGroupHandle_t wifi_event_group = NULL;
#define CONNECTED_BIT BIT0


// 单例实例
UartSender *UartSender::instance_ = nullptr;

// 静态字符串变量
std::string UartSender::ascii_str_part1;
std::string UartSender::ascii_str_part2;

static const char *TAG = "UART_SENDER_1";
static wifi_config_t sta_config;
static bool gl_sta_got_ip = false;
static bool ssid_sta = false;
static bool passd_sta = false;
int Volince_flag = 0;
int Volince_count = 0;
// 在uartcmdsend.cc中需要定义静态成员
// std::string UartSender::static_ascii_str_part1;
// std::string UartSender::static_ascii_str_part2;

// 构造函数
UartSender::UartSender(uart_port_t uart_num, const uart_config_t &uart_config)
    : uart_num_(uart_num), uart_config_(uart_config), rx_buffer_index_(0), last_rx_time_(0),
      frame_state_(FrameState::IDLE), frame_buffer_index_(0)
{

    // 配置UART参数
    ESP_ERROR_CHECK(uart_param_config(uart_num_, &uart_config_));

    // 设置UART引脚
    ESP_ERROR_CHECK(uart_set_pin(uart_num_, ECHO_TEST_TXD, ECHO_TEST_RXD,
                                 ECHO_TEST_RTS, ECHO_TEST_CTS));

    // 安装UART驱动
    ESP_ERROR_CHECK(uart_driver_install(uart_num_, 256, 0, 0, nullptr, 0));

    ESP_LOGI(TAG, "UART在端口 %d 初始化完成", uart_num_);


    // 以下是定时器版本的接收检查（目前注释掉）
    // esp_timer_create_args_t clock_timer_args = {
    //     .callback = [](void *arg)
    //     {
    //         UartSender *sender = static_cast<UartSender *>(arg);
    //         sender->checkForReceivedData(); 
    //     },
    //     .arg = this,
    //     .dispatch_method = ESP_TIMER_TASK,
    //     .name = "clock_timer",
    //     .skip_unhandled_events = true};
    // esp_timer_create(&clock_timer_args, &clock_timer_handle_);
    // esp_timer_start_periodic(clock_timer_handle_, 1000000);
    
    // 创建任务用于检查接收数据
    xTaskCreate([](void* arg) {
    auto sender = static_cast<UartSender*>(arg);
    while(1) {
        sender->checkForReceivedData();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}, "uart_task", 4096, this, 5, nullptr);
 
    // 设置默认的接收回调函数
    setReceiveCallback([](const std::vector<uint8_t> &data)
                       {
                           // 默认空实现
                       });
}

// 析构函数
UartSender::~UartSender()
{
    if (instance_)
    {
        // 停止并删除定时器
        if (clock_timer_handle_)
        {
            esp_timer_stop(clock_timer_handle_);
            esp_timer_delete(clock_timer_handle_);
            clock_timer_handle_ = nullptr;
        }

        // 卸载UART驱动
        uart_driver_delete(uart_num_);
        delete instance_;
        instance_ = nullptr;
        ESP_LOGI(TAG, "UART驱动已删除");
    }
}

// 初始化单例实例
void UartSender::Initialize(uart_port_t uart_num, const uart_config_t &uart_config)
{
    if (!instance_)
    {
        instance_ = new UartSender(uart_num, uart_config);
    }
    else
    {
        ESP_LOGW(TAG, "UART已在端口 %d 初始化", instance_->uart_num_);
    }
}

// 获取单例实例
UartSender &UartSender::GetInstance()
{
    if (!instance_)
    {
        // 如果未初始化，记录错误
        ESP_LOGE(TAG, "UART未初始化！请先调用Initialize()");

        // 使用默认配置进行初始化
        uart_config_t default_config = {
            .baud_rate = 9600,
            .data_bits = UART_DATA_8_BITS,
            .parity = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .rx_flow_ctrl_thresh = 122,
        };

        UartSender::Initialize(UART_NUM_1, default_config);
    }
    return *instance_;
}

// 发送字符串数据
void UartSender::sendString(const std::string &send_data)
{
    if (!instance_)
    {
        ESP_LOGE(TAG, "UART未初始化！");
        return;
    }
    // 发送数据
    int bytes_sent = uart_write_bytes(uart_num_, send_data.c_str(), send_data.length());

    if (bytes_sent >= 0)
    {
        // ESP_LOGI(TAG, "已发送 %d 字节: %s", bytes_sent, send_data.c_str());
    }
    else
    {
        ESP_LOGE(TAG, "发送数据失败");
    }
}

// 发送十六进制数据（单个字节）
void UartSender::sendHex(uint8_t hexValue)
{
    if (!instance_)
    {
        ESP_LOGE(TAG, "UART未初始化！");
        return;
    }

    // 发送单个字节的十六进制数据
    int bytes_sent = uart_write_bytes(uart_num_, &hexValue, 1);

    if (bytes_sent >= 0)
    {
        // 以0xXX格式打印发送的十六进制值
        ESP_LOGI(TAG, "已发送十六进制: 0x%02X", hexValue);
    }
    else
    {
        ESP_LOGE(TAG, "发送十六进制值失败: 0x%02X", hexValue);
    }
}

// 发送数字（转换为字节）
void UartSender::sendNumber(int number)
{
    if (!instance_)
    {
        ESP_LOGE(TAG, "UART未初始化！");
        return;
    }

    // 将数字转换为字节数据
    uint8_t byteData = static_cast<uint8_t>(number);

    // 发送字节数据
    int bytes_sent = uart_write_bytes(uart_num_, &byteData, 1);

    if (bytes_sent >= 0)
    {
        ESP_LOGI(TAG, "已发送数字: %d", number);
    }
    else
    {
        ESP_LOGE(TAG, "发送数字失败");
    }
}

// 发送实时数据（带CRC校验）
void UartSender::sendRealTimeData(const std::vector<uint8_t> &realTimeData)
{
    if (!instance_)
    {
        ESP_LOGE(TAG, "UART未初始化！");
        return;
    }

    // 构建实时数据帧：起始符0x3A + 数据 + CRC + 结束符0x7E
    std::vector<uint8_t> realTimeFrame;
    realTimeFrame.push_back(0x3A); // 起始符

    // 检查数据是否为空
    if (realTimeData.empty())
    {
        ESP_LOGE(TAG, "实时数据为空！");
        return;
    }

    // 添加数据部分
    realTimeFrame.insert(realTimeFrame.end(), realTimeData.begin(), realTimeData.end());

    // 计算CRC校验值并添加到帧中
    uint8_t crc = calculateCRC(realTimeData.data(), realTimeData.size());
    realTimeFrame.push_back(crc);  // 添加CRC值
    realTimeFrame.push_back(0x7E); // 结束符

    // 发送帧数据
    int bytes_sent = uart_write_bytes(uart_num_, realTimeFrame.data(), realTimeFrame.size());
    if (bytes_sent >= 0)
    {
        ESP_LOGI(TAG, "已发送实时数据帧: %d 字节", bytes_sent);
        // 打印发送的帧内容
        for (uint8_t b : realTimeFrame)
        {
            // printf("RealTime Sent: 0x%02X ", b);
            printf("0x%02X ", b);
        }
        printf("\n");
    }
    else
    {
        ESP_LOGE(TAG, "发送实时数据失败");
    }
}

// 设置接收回调函数
void UartSender::setReceiveCallback(std::function<void(const std::vector<uint8_t> &)> callback)
{
    receive_callback_ = callback;
}

// 初始化GPIO
void UartSender::initGpio() {
    // 配置AUDIO_CODEC_PA_PIN为输出
    gpio_set_direction(GPIO_NUM_21, GPIO_MODE_OUTPUT);
    ESP_LOGI(TAG, "AUDIO_CODEC_PA_PIN的GPIO已初始化: %d", GPIO_NUM_21);
}

// 发送帧数据（UartSender内部使用）
void UartSender::sendFrame(const std::vector<uint8_t> &data)
{
    if (!instance_)
        return;

    // 构建帧：起始符0x3A + 数据 + CRC + 结束符0x7E
    std::vector<uint8_t> frame;
    frame.push_back(0x3A); // 起始符

    frame.insert(frame.end(), data.begin(), data.end()); // 添加数据部分
    
    // 计算CRC校验值
    uint8_t crc = calculateCRC(data.data(), data.size());

    frame.push_back(crc);  // 添加CRC值
    frame.push_back(0x7E); // 结束符

    // 发送帧
    int bytes_sent = uart_write_bytes(uart_num_, frame.data(), frame.size());
    if (bytes_sent >= 0)
    {
        ESP_LOGI(TAG, "已发送帧: %d 字节", bytes_sent);
        // 打印发送的帧内容
        for (uint8_t b : frame)
        {
            printf("Sent: 0x%02X ", b);
        }
        printf("\n");
    }
    else
    {
        ESP_LOGE(TAG, "发送帧失败");
    }
}

// 发送自定义数据（外部调用接口）
void UartSender::sendCustomData(const std::vector<uint8_t> &customData)
{
    if (!instance_)
    {
        ESP_LOGE(TAG, "UART未初始化！");
        return;
    }

    // 调用sendFrame发送自定义数据，自动处理CRC计算
    sendFrame(customData);
}


// 检查接收数据
void UartSender::checkForReceivedData()
{
    if (!instance_)
    {
        ESP_LOGE(TAG, "UART未初始化！");
        return;
    }

    // 获取当前时间（毫秒）
    uint32_t current_time = esp_timer_get_time() / 1000;

    // 检查接收缓冲区中的字节数
    size_t rx_bytes = 0;
    esp_err_t err = uart_get_buffered_data_len(uart_num_, &rx_bytes);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "获取UART缓冲数据长度失败: %d", err);
        return;
    }

    if (rx_bytes > 0)
    {
        // 读取数据
        uint8_t data[BUF_SIZE];
        int length = uart_read_bytes(uart_num_, data, rx_bytes, 0);
        if (length > 0)
        {
            // 更新最后接收时间
            last_rx_time_ = current_time;

            for (int i = 0; i < length; i++)
            {
                printf("data[%d] = %d (0x%02X)\n", i, data[i], (unsigned char)data[i]);
            }

            // 处理每个字节
            for (int i = 0; i < length; i++)
            {
                processByte(data[i]);
            }
        }
    }
    else if (frame_buffer_index_ > 0 && (current_time - last_rx_time_) > UART_RX_TIMEOUT)
    {
        // 帧接收超时，重置解析器
        ESP_LOGW(TAG, "帧接收超时，重置解析器");
        resetFrameParser();
    }
}


// 计算CRC校验值（取低8位）
uint8_t UartSender::calculateCRC(const uint8_t *data, size_t length)
{
    uint8_t crc = 0;
    printf("length = %d\n", length);
    // 打印校验数据
    for (size_t i = 0; i < length; i++)
    {
        //printf("校验数据[%d] = %d (0x%02X)\n", i, data[i], (unsigned char)data[i]);
    }

    for (size_t i = 0; i < length; i++)
    {
        crc += data[i];
    }
    return crc & 0xFF;
}
int Volincelisting_flag = 0;
int Vonum_flag = 0;
int Uart_flag = 0;
int Passed_flag = 0;
bool connectResult = 0;

uint8_t Text_flag1 = 0;

// 根据命令发送响应
void UartSender::sendResponseByCommand(uint8_t cmd, uint8_t cmd4, uint8_t cmd5, const std::string& chinese_utf8)
{
     ESP_LOGI(TAG, "中文字符串(UTF-8)132456: %s", chinese_utf8.c_str());
    //  ESP_LOGE(TAG, "进入处理!");
    std::vector<uint8_t> responseData;
    switch (cmd) //例如:3A 03 01 0x 0y 00 09 0D 7E
    {
        case 0x03: // 处理命令0x03
        {

            // auto display = Board::GetInstance().GetDisplay();

                // 添加日志验证字符串内容和长度

            // ESP_LOGE(TAG, "命令: 0x%02X, 中文文本: %s, 对齐方式: %d, X偏移: %d, Y偏移: %d",cmd, chinese_utf8.c_str(), LV_ALIGN_TOP_LEFT, cmd4, cmd5);
            // display->Settext1(chinese_utf8.c_str(),LV_ALIGN_TOP_LEFT,cmd4,cmd5);

            // display->Settext1("显示成功",LV_ALIGN_TOP_LEFT,cmd4,cmd5+13);

            // display->Settext("显示成功",LV_ALIGN_TOP_LEFT,cmd4,cmd5+13);
            break;
        }
        case 0x05: // 处理0x04命令
        {
            // auto display = Board::GetInstance().GetDisplay();
            // display->ShowSdImage("E:/text1.bmp");
            // break;
        }

                break;
        default:
            break;
    }


}
// 将十六进制字符串转换为ASCII字符串（例如"48 65 6C 6C 6F" -> "Hello"）
std::string UartSender::HexStringToAscii(const std::string &hex_str)
{
    std::string result;
    std::stringstream ss(hex_str);
    std::string byte_str;

    while (std::getline(ss, byte_str, ' '))
    { // 按空格分割十六进制字符串
        if (byte_str.size() != 2)
        {
            ESP_LOGW(TAG, "无效的十六进制字节: %s", byte_str.c_str());
            continue;
        }

        char byte = static_cast<char>(std::stoi(byte_str, nullptr, 16));
        result += byte;
    }

    return result;
}

// 将十六进制字节数组转换为ASCII字符串
std::string UartSender::HexBytesToAscii(const std::vector<uint8_t> &hex_bytes)
{
    std::string result;
    result.reserve(hex_bytes.size()); // 预分配内存提高效率

    for (uint8_t byte : hex_bytes)
    {
        // 只转换可打印的ASCII字符（0x20~0x7E）
        if (byte >= 0x20 && byte <= 0x7E)
        {
            result += static_cast<char>(byte);
        }
        else
        {
            result += '.'; // 非ASCII字符用点代替
            ESP_LOGD(TAG, "非ASCII字节: 0x%02X", byte);
        }
    }

    return result;
}

// 处理单个接收字节
void UartSender::processByte(uint8_t byte)
{
    // printf("byte = %d (0x%02X)\n", byte, (unsigned char)byte);
    switch (frame_state_)
    {
    case FrameState::IDLE:
        // 等待起始符 0x3A
        if (byte == 0x3A)
        {
            printf("byte == 0x3A\n");
            frame_state_ = FrameState::RECEIVING;
            frame_buffer_index_ = 0;
            // 开始接收新帧
            ESP_LOGD(TAG, "帧开始");
        }
        break;

    case FrameState::RECEIVING:
        // 检查是否收到结束符 0x7E
        if (byte == 0x7E)
        {
            printf("11byte == 0x7E\n");
            // 帧接收完成
            frame_state_ = FrameState::IDLE;

            uint16_t n = (frame_buffer_[3] << 8) | frame_buffer_[4]; // n是帧长度（从数据手册获取的定义）
            printf("n = %d\n", n);

            // 检查是否有足够的数据进行CRC验证
            if (frame_buffer_index_ >= 5)
            { // 至少需要5个字节才可能是有效帧
                // 获取接收的CRC值
                uint8_t receivedCRC = frame_buffer_[frame_buffer_index_ - 1];

                // 计算接收数据的CRC值
                uint8_t calculatedCRC = calculateCRC(frame_buffer_, frame_buffer_index_ - 1);

                int lendflag = 0; // 长度验证标志

                int start_index = 4;                     // 数据部分起始索引（根据协议定义）
                int crc_index = frame_buffer_index_ - 1; // CRC值所在索引
                int Length_validation = 0;               // 长度验证值

                if (start_index < crc_index)
                {
                    // 计算数据部分长度
                    int byte_count = crc_index - start_index - 1;
                    Length_validation = byte_count + 8;
                    ESP_LOGI(TAG, "数据部分到CRC前的长度: %d", byte_count);
                }
                else
                {
                    ESP_LOGW(TAG, "数据索引异常");
                }

                if(Length_validation == n)
                {
                    // 长度验证通过
                    ESP_LOGI(TAG, "长度验证通过");
                    lendflag = 1;
                    
                }
                else
                {
                    ESP_LOGE(TAG, "长度验证失败");
                    printf("Length_validation = %d\n", Length_validation);
                    printf("n = %d\n", n);
                    lendflag = 0;

                }

                // 验证CRC
                if (receivedCRC == calculatedCRC)
                {
                    if(lendflag == 1)
                    {
                            ESP_LOGI(TAG, "CRC校验通过! 接收: 0x%02X, 计算: 0x%02X",
                            receivedCRC, calculatedCRC);

                            // 解析命令（假设在数据的特定位置）
                            if (frame_buffer_index_ >= 5)
                            {
                                uint8_t cmd = frame_buffer_[1];  // 命令字（位置1）
                                uint8_t cmd4 = frame_buffer_[5]; // 子命令1（位置5）
                                uint8_t cmd5 = frame_buffer_[6]; // 子命令2（位置6）
                                ESP_LOGI(TAG, "收到命令: 0x%02X", cmd);
                                ESP_LOGI(TAG, "收到子命令4: 0x%02X", cmd4);
                                ESP_LOGI(TAG, "收到子命令5: 0x%02X", cmd5);

                                // 提取中文字符（UTF-8）
                                std::string chinese_utf8;
                                
                                // 中文字符从cmd5之后开始，到CRC之前结束
                                int chinese_start_index = 7;  // 中文开始索引（位置7）
                                int chinese_end_index = crc_index - 1;  // 中文结束索引

                                if(cmd == 0xa0)
                                {
                                        if (chinese_start_index <= chinese_end_index)
                                        {
                                            int chinese_byte_count = chinese_end_index - chinese_start_index + 1;

                                            // 提取UTF-8字节
                                            uint8_t* chinese_bytes = &frame_buffer_[chinese_start_index];
                                            std::string chinese_utf8(reinterpret_cast<char*>(chinese_bytes), chinese_byte_count);

                                            // 打印中文字节
                                            ESP_LOGI(TAG, "中文UTF-8字节序列:");
                                            for (int i = 0; i < chinese_byte_count; i++)
                                            {
                                                printf("0x%02X ", chinese_bytes[i]);
                                                if ((i + 1) % 16 == 0) printf("\n");
                                            }
                                            printf("\n");
                                            // ... 构建 chinese_utf8
                                            ESP_LOGI(TAG, "processByte: 字符串长度: %d", chinese_utf8.size());
                                            for (size_t i = 0; i < chinese_utf8.size(); i++) {
                                                ESP_LOGI(TAG, "processByte: 字符[%d]: 0x%02X", i, (unsigned char)chinese_utf8[i]);
                                            }
                                            ESP_LOGI(TAG, "processByte: 中文字符串(UTF-8): %s", chinese_utf8.c_str());


                                            auto display = Board::GetInstance().GetDisplay();
                                            display->Show();
                                            display->Settext1(chinese_utf8.c_str(),LV_ALIGN_TOP_LEFT,cmd4,0x00);


                                        }                                
                                }
                                else if(cmd == 0xa1)
                                {
                                        if (chinese_start_index <= chinese_end_index)
                                        {
                                            int chinese_byte_count = chinese_end_index - chinese_start_index + 1;

                                            // 提取UTF-8字节
                                            uint8_t* chinese_bytes = &frame_buffer_[chinese_start_index];
                                            std::string chinese_utf8(reinterpret_cast<char*>(chinese_bytes), chinese_byte_count);

                                            // 打印中文字节
                                            ESP_LOGI(TAG, "中文UTF-8字节序列:");
                                            for (int i = 0; i < chinese_byte_count; i++)
                                            {
                                                printf("0x%02X ", chinese_bytes[i]);
                                                if ((i + 1) % 16 == 0) printf("\n");
                                            }
                                            printf("\n");
                                            // ... 构建 chinese_utf8
                                            ESP_LOGI(TAG, "processByte: 字符串长度: %d", chinese_utf8.size());
                                            for (size_t i = 0; i < chinese_utf8.size(); i++) {
                                                ESP_LOGI(TAG, "processByte: 字符[%d]: 0x%02X", i, (unsigned char)chinese_utf8[i]);
                                            }
                                            ESP_LOGI(TAG, "processByte: 中文字符串(UTF-8): %s", chinese_utf8.c_str());


                                            auto display = Board::GetInstance().GetDisplay();
                                            display->Show();
                                            display->Settext(chinese_utf8.c_str(),LV_ALIGN_TOP_LEFT,cmd4,0x14);
                                        } 
                                }
                                else if(cmd == 0xa2)
                                {
                                        if (chinese_start_index <= chinese_end_index)
                                        {
                                            int chinese_byte_count = chinese_end_index - chinese_start_index + 1;

                                            // 提取UTF-8字节
                                            uint8_t* chinese_bytes = &frame_buffer_[chinese_start_index];
                                            std::string chinese_utf8(reinterpret_cast<char*>(chinese_bytes), chinese_byte_count);

                                            // 打印中文字节
                                            ESP_LOGI(TAG, "中文UTF-8字节序列:");
                                            for (int i = 0; i < chinese_byte_count; i++)
                                            {
                                                printf("0x%02X ", chinese_bytes[i]);
                                                if ((i + 1) % 16 == 0) printf("\n");
                                            }
                                            printf("\n");
                                            // ... 构建 chinese_utf8
                                            ESP_LOGI(TAG, "processByte: 字符串长度: %d", chinese_utf8.size());
                                            for (size_t i = 0; i < chinese_utf8.size(); i++) {
                                                ESP_LOGI(TAG, "processByte: 字符[%d]: 0x%02X", i, (unsigned char)chinese_utf8[i]);
                                            }
                                            ESP_LOGI(TAG, "processByte: 中文字符串(UTF-8): %s", chinese_utf8.c_str());


                                            auto display = Board::GetInstance().GetDisplay();
                                            display->Show();
                                            display->Settext2(chinese_utf8.c_str(),LV_ALIGN_TOP_LEFT,cmd4,0x27);
                                        } 
                                }
                                else if(cmd == 0xa3)
                                {
                                        if (chinese_start_index <= chinese_end_index)
                                        {
                                            int chinese_byte_count = chinese_end_index - chinese_start_index + 1;

                                            // 提取UTF-8字节
                                            uint8_t* chinese_bytes = &frame_buffer_[chinese_start_index];
                                            std::string chinese_utf8(reinterpret_cast<char*>(chinese_bytes), chinese_byte_count);

                                            // 打印中文字节
                                            ESP_LOGI(TAG, "中文UTF-8字节序列:");
                                            for (int i = 0; i < chinese_byte_count; i++)
                                            {
                                                printf("0x%02X ", chinese_bytes[i]);
                                                if ((i + 1) % 16 == 0) printf("\n");
                                            }
                                            printf("\n");
                                            // ... 构建 chinese_utf8
                                            ESP_LOGI(TAG, "processByte: 字符串长度: %d", chinese_utf8.size());
                                            for (size_t i = 0; i < chinese_utf8.size(); i++) {
                                                ESP_LOGI(TAG, "processByte: 字符[%d]: 0x%02X", i, (unsigned char)chinese_utf8[i]);
                                            }
                                            ESP_LOGI(TAG, "processByte: 中文字符串(UTF-8): %s", chinese_utf8.c_str());


                                            auto display = Board::GetInstance().GetDisplay();
                                            display->Show();
                                            display->Settext3(chinese_utf8.c_str(),LV_ALIGN_TOP_LEFT,cmd4,0x3a);
                                        } 
                                }
                                else if(cmd == 0xa4)
                                {
                                        if (chinese_start_index <= chinese_end_index)
                                        {
                                            int chinese_byte_count = chinese_end_index - chinese_start_index + 1;

                                            // 提取UTF-8字节
                                            uint8_t* chinese_bytes = &frame_buffer_[chinese_start_index];
                                            std::string chinese_utf8(reinterpret_cast<char*>(chinese_bytes), chinese_byte_count);

                                            // 打印中文字节
                                            ESP_LOGI(TAG, "中文UTF-8字节序列:");
                                            for (int i = 0; i < chinese_byte_count; i++)
                                            {
                                                printf("0x%02X ", chinese_bytes[i]);
                                                if ((i + 1) % 16 == 0) printf("\n");
                                            }
                                            printf("\n");
                                            // ... 构建 chinese_utf8
                                            ESP_LOGI(TAG, "processByte: 字符串长度: %d", chinese_utf8.size());
                                            for (size_t i = 0; i < chinese_utf8.size(); i++) {
                                                ESP_LOGI(TAG, "processByte: 字符[%d]: 0x%02X", i, (unsigned char)chinese_utf8[i]);
                                            }
                                            ESP_LOGI(TAG, "processByte: 中文字符串(UTF-8): %s", chinese_utf8.c_str());


                                            auto display = Board::GetInstance().GetDisplay();
                                            display->Show();
                                            display->Settext4(chinese_utf8.c_str(),LV_ALIGN_TOP_LEFT,cmd4,0x4d);
                                        } 
                                }
                                else if(cmd == 0xa5)
                                {
                                        if (chinese_start_index <= chinese_end_index)
                                        {
                                            int chinese_byte_count = chinese_end_index - chinese_start_index + 1;

                                            // 提取UTF-8字节
                                            uint8_t* chinese_bytes = &frame_buffer_[chinese_start_index];
                                            std::string chinese_utf8(reinterpret_cast<char*>(chinese_bytes), chinese_byte_count);

                                            // 打印中文字节
                                            ESP_LOGI(TAG, "中文UTF-8字节序列:");
                                            for (int i = 0; i < chinese_byte_count; i++)
                                            {
                                                printf("0x%02X ", chinese_bytes[i]);
                                                if ((i + 1) % 16 == 0) printf("\n");
                                            }
                                            printf("\n");
                                            // ... 构建 chinese_utf8
                                            ESP_LOGI(TAG, "processByte: 字符串长度: %d", chinese_utf8.size());
                                            for (size_t i = 0; i < chinese_utf8.size(); i++) {
                                                ESP_LOGI(TAG, "processByte: 字符[%d]: 0x%02X", i, (unsigned char)chinese_utf8[i]);
                                            }
                                            ESP_LOGI(TAG, "processByte: 中文字符串(UTF-8): %s", chinese_utf8.c_str());


                                            auto display = Board::GetInstance().GetDisplay();
                                            display->Show();
                                            display->Settext5(chinese_utf8.c_str(),LV_ALIGN_TOP_LEFT,cmd4,0x60);
                                        } 
                                }                                                                 
                                else if(cmd == 0xa6)
                                {
                                        if (chinese_start_index <= chinese_end_index)
                                        {
                                            int chinese_byte_count = chinese_end_index - chinese_start_index + 1;

                                            // 提取UTF-8字节
                                            uint8_t* chinese_bytes = &frame_buffer_[chinese_start_index];
                                            std::string chinese_utf8(reinterpret_cast<char*>(chinese_bytes), chinese_byte_count);

                                            // 打印中文字节
                                            ESP_LOGI(TAG, "中文UTF-8字节序列:");
                                            for (int i = 0; i < chinese_byte_count; i++)
                                            {
                                                printf("0x%02X ", chinese_bytes[i]);
                                                if ((i + 1) % 16 == 0) printf("\n");
                                            }
                                            printf("\n");
                                            // ... 构建 chinese_utf8
                                            ESP_LOGI(TAG, "processByte: 字符串长度: %d", chinese_utf8.size());
                                            for (size_t i = 0; i < chinese_utf8.size(); i++) {
                                                ESP_LOGI(TAG, "processByte: 字符[%d]: 0x%02X", i, (unsigned char)chinese_utf8[i]);
                                            }
                                            ESP_LOGI(TAG, "processByte: 中文字符串(UTF-8): %s", chinese_utf8.c_str());


                                            auto display = Board::GetInstance().GetDisplay();
                                            display->Show();
                                            display->Settext6(chinese_utf8.c_str(),LV_ALIGN_TOP_LEFT,cmd4,0x73);
                                        } 
                                } 
                                else if(cmd == 0xa7)
                                {
                                        if (chinese_start_index <= chinese_end_index)
                                        {
                                            int chinese_byte_count = chinese_end_index - chinese_start_index + 1;

                                            // 提取UTF-8字节
                                            uint8_t* chinese_bytes = &frame_buffer_[chinese_start_index];
                                            std::string chinese_utf8(reinterpret_cast<char*>(chinese_bytes), chinese_byte_count);

                                            // 打印中文字节
                                            ESP_LOGI(TAG, "中文UTF-8字节序列:");
                                            for (int i = 0; i < chinese_byte_count; i++)
                                            {
                                                printf("0x%02X ", chinese_bytes[i]);
                                                if ((i + 1) % 16 == 0) printf("\n");
                                            }
                                            printf("\n");
                                            // ... 构建 chinese_utf8
                                            ESP_LOGI(TAG, "processByte: 字符串长度: %d", chinese_utf8.size());
                                            for (size_t i = 0; i < chinese_utf8.size(); i++) {
                                                ESP_LOGI(TAG, "processByte: 字符[%d]: 0x%02X", i, (unsigned char)chinese_utf8[i]);
                                            }
                                            ESP_LOGI(TAG, "processByte: 中文字符串(UTF-8): %s", chinese_utf8.c_str());


                                            auto display = Board::GetInstance().GetDisplay();
                                            display->Show();
                                            display->Settext7(chinese_utf8.c_str(),LV_ALIGN_TOP_LEFT,cmd4,0x86);
                                        } 
                                }
                                else if(cmd == 0xa8)
                                {
                                        if (chinese_start_index <= chinese_end_index)
                                        {
                                            int chinese_byte_count = chinese_end_index - chinese_start_index + 1;

                                            // 提取UTF-8字节
                                            uint8_t* chinese_bytes = &frame_buffer_[chinese_start_index];
                                            std::string chinese_utf8(reinterpret_cast<char*>(chinese_bytes), chinese_byte_count);

                                            // 打印中文字节
                                            ESP_LOGI(TAG, "中文UTF-8字节序列:");
                                            for (int i = 0; i < chinese_byte_count; i++)
                                            {
                                                printf("0x%02X ", chinese_bytes[i]);
                                                if ((i + 1) % 16 == 0) printf("\n");
                                            }
                                            printf("\n");
                                            // ... 构建 chinese_utf8
                                            ESP_LOGI(TAG, "processByte: 字符串长度: %d", chinese_utf8.size());
                                            for (size_t i = 0; i < chinese_utf8.size(); i++) {
                                                ESP_LOGI(TAG, "processByte: 字符[%d]: 0x%02X", i, (unsigned char)chinese_utf8[i]);
                                            }
                                            ESP_LOGI(TAG, "processByte: 中文字符串(UTF-8): %s", chinese_utf8.c_str());


                                            auto display = Board::GetInstance().GetDisplay();
                                            display->Show();
                                            display->Settext8(chinese_utf8.c_str(),LV_ALIGN_TOP_LEFT,cmd4,0x99);
                                        } 
                                }  
                                else if(cmd == 0xa9)
                                {
                                        if (chinese_start_index <= chinese_end_index)
                                        {
                                            int chinese_byte_count = chinese_end_index - chinese_start_index + 1;

                                            // 提取UTF-8字节
                                            uint8_t* chinese_bytes = &frame_buffer_[chinese_start_index];
                                            std::string chinese_utf8(reinterpret_cast<char*>(chinese_bytes), chinese_byte_count);

                                            // 打印中文字节
                                            ESP_LOGI(TAG, "中文UTF-8字节序列:");
                                            for (int i = 0; i < chinese_byte_count; i++)
                                            {
                                                printf("0x%02X ", chinese_bytes[i]);
                                                if ((i + 1) % 16 == 0) printf("\n");
                                            }
                                            printf("\n");
                                            // ... 构建 chinese_utf8
                                            ESP_LOGI(TAG, "processByte: 字符串长度: %d", chinese_utf8.size());
                                            for (size_t i = 0; i < chinese_utf8.size(); i++) {
                                                ESP_LOGI(TAG, "processByte: 字符[%d]: 0x%02X", i, (unsigned char)chinese_utf8[i]);
                                            }
                                            ESP_LOGI(TAG, "processByte: 中文字符串(UTF-8): %s", chinese_utf8.c_str());


                                            auto display = Board::GetInstance().GetDisplay();
                                            display->Show();
                                            display->Settext9(chinese_utf8.c_str(),LV_ALIGN_TOP_LEFT,cmd4,0xac);
                                        } 
                                }  
                                else if(cmd == 0xaa)
                                {
                                        if (chinese_start_index <= chinese_end_index)
                                        {
                                            int chinese_byte_count = chinese_end_index - chinese_start_index + 1;

                                            // 提取UTF-8字节
                                            uint8_t* chinese_bytes = &frame_buffer_[chinese_start_index];
                                            std::string chinese_utf8(reinterpret_cast<char*>(chinese_bytes), chinese_byte_count);

                                            // 打印中文字节
                                            ESP_LOGI(TAG, "中文UTF-8字节序列:");
                                            for (int i = 0; i < chinese_byte_count; i++)
                                            {
                                                printf("0x%02X ", chinese_bytes[i]);
                                                if ((i + 1) % 16 == 0) printf("\n");
                                            }
                                            printf("\n");
                                            // ... 构建 chinese_utf8
                                            ESP_LOGI(TAG, "processByte: 字符串长度: %d", chinese_utf8.size());
                                            for (size_t i = 0; i < chinese_utf8.size(); i++) {
                                                ESP_LOGI(TAG, "processByte: 字符[%d]: 0x%02X", i, (unsigned char)chinese_utf8[i]);
                                            }
                                            ESP_LOGI(TAG, "processByte: 中文字符串(UTF-8): %s", chinese_utf8.c_str());


                                            auto display = Board::GetInstance().GetDisplay();
                                            display->Show();
                                            display->Settext10(chinese_utf8.c_str(),LV_ALIGN_TOP_LEFT,cmd4,0xbf);
                                        } 
                                }
                                else if(cmd == 0xab)
                                {
                                        if (chinese_start_index <= chinese_end_index)
                                        {
                                            int chinese_byte_count = chinese_end_index - chinese_start_index + 1;

                                            // 提取UTF-8字节
                                            uint8_t* chinese_bytes = &frame_buffer_[chinese_start_index];
                                            std::string chinese_utf8(reinterpret_cast<char*>(chinese_bytes), chinese_byte_count);

                                            // 打印中文字节
                                            ESP_LOGI(TAG, "中文UTF-8字节序列:");
                                            for (int i = 0; i < chinese_byte_count; i++)
                                            {
                                                printf("0x%02X ", chinese_bytes[i]);
                                                if ((i + 1) % 16 == 0) printf("\n");
                                            }
                                            printf("\n");
                                            // ... 构建 chinese_utf8
                                            ESP_LOGI(TAG, "processByte: 字符串长度: %d", chinese_utf8.size());
                                            for (size_t i = 0; i < chinese_utf8.size(); i++) {
                                                ESP_LOGI(TAG, "processByte: 字符[%d]: 0x%02X", i, (unsigned char)chinese_utf8[i]);
                                            }
                                            ESP_LOGI(TAG, "processByte: 中文字符串(UTF-8): %s", chinese_utf8.c_str());


                                            auto display = Board::GetInstance().GetDisplay();
                                            display->Show();
                                            display->Settext12(chinese_utf8.c_str(),LV_ALIGN_TOP_LEFT,cmd4,0xd2);
                                        } 
                                } 
                                else if(cmd == 0xac)
                                {   
                                        if (chinese_start_index <= chinese_end_index)
                                        {
                                            int chinese_byte_count = chinese_end_index - chinese_start_index + 1;

                                            // 提取UTF-8字节
                                            uint8_t* chinese_bytes = &frame_buffer_[chinese_start_index];
                                            std::string chinese_utf8(reinterpret_cast<char*>(chinese_bytes), chinese_byte_count);

                                            // 打印中文字节
                                            ESP_LOGI(TAG, "中文UTF-8字节序列:");
                                            for (int i = 0; i < chinese_byte_count; i++)
                                            {
                                                printf("0x%02X ", chinese_bytes[i]);
                                                if ((i + 1) % 16 == 0) printf("\n");
                                            }
                                            printf("\n");
                                            // ... 构建 chinese_utf8
                                            ESP_LOGI(TAG, "processByte: 字符串长度: %d", chinese_utf8.size());
                                            for (size_t i = 0; i < chinese_utf8.size(); i++) {
                                                ESP_LOGI(TAG, "processByte: 字符[%d]: 0x%02X", i, (unsigned char)chinese_utf8[i]);
                                            }
                                            ESP_LOGI(TAG, "processByte: 中文字符串(UTF-8): %s", chinese_utf8.c_str());


                                            auto display = Board::GetInstance().GetDisplay();
                                            display->Show();
                                            display->Settext12(chinese_utf8.c_str(),LV_ALIGN_TOP_LEFT,cmd4,cmd5);
                                        } 
                                }                                                                                                                                                                                               
                                else if(cmd == 0x01)
                                {
                                            auto display = Board::GetInstance().GetDisplay();
                                            display->ShowImage();
                                            display->ShowSdImage("E:/01.png");
                                }
                                else if(cmd == 0x02)
                                {
                                            auto display = Board::GetInstance().GetDisplay();
                                            display->ShowImage();
                                            display->ShowSdImage("E:/02.png");
                                }
                                else if(cmd == 0x03)
                                {
                                            auto display = Board::GetInstance().GetDisplay();
                                            display->ShowImage();
                                            display->ShowSdImage("E:/03.png");
                                }
                                else if(cmd == 0x04)
                                {
                                            auto display = Board::GetInstance().GetDisplay();
                                            display->ShowImage();
                                            display->ShowSdImage("E:/04.png");
                                }
                                else if(cmd == 0x05)
                                {
                                            auto display = Board::GetInstance().GetDisplay();
                                            display->ShowImage();
                                            display->ShowSdImage("E:/05.png");
                                }
                                else if(cmd == 0x06)
                                {
                                            auto display = Board::GetInstance().GetDisplay();
                                            display->ShowImage();
                                            display->ShowSdImage("E:/06.png");
                                }
                                else if(cmd == 0x07)
                                {
                                            auto display = Board::GetInstance().GetDisplay();
                                            display->ShowImage();
                                            display->ShowSdImage("E:/07.png");
                                }
                                else if(cmd == 0x08)
                                {
                                            auto display = Board::GetInstance().GetDisplay();
                                            display->ShowImage();
                                            display->ShowSdImage("E:/08.png");
                                }
                                else if(cmd == 0x09)
                                {
                                            auto display = Board::GetInstance().GetDisplay();
                                            display->ShowImage();
                                            display->ShowSdImage("E:/09.png");
                                }
                                else if(cmd == 0x10)
                                {
                                            auto display = Board::GetInstance().GetDisplay();
                                            display->ShowImage();
                                            display->ShowSdImage("E:/10.png");
                                }
                                else if(cmd == 0x11)
                                {
                                            auto display = Board::GetInstance().GetDisplay();
                                            display->ShowImage();
                                            display->ShowSdImage("E:/11.png");
                                }
                                else if(cmd == 0x12)
                                {
                                            auto display = Board::GetInstance().GetDisplay();
                                            display->ShowImage();
                                            display->ShowSdImage("E:/12.png");
                                }
                                else if(cmd == 0x13)
                                {
                                            auto display = Board::GetInstance().GetDisplay();
                                            display->ShowImage();
                                            display->ShowSdImage("E:/13.png");
                                }
                                else if(cmd == 0x14)
                                {
                                            auto display = Board::GetInstance().GetDisplay();
                                            display->ShowImage();
                                            display->ShowSdImage("E:/14.png");
                                }
                                else if(cmd == 0x15)
                                {
                                            auto display = Board::GetInstance().GetDisplay();
                                            display->ShowImage();
                                            display->ShowSdImage("E:/15.png");
                                }
                                else if(cmd == 0x16)
                                {
                                            auto display = Board::GetInstance().GetDisplay();
                                            display->ShowImage();
                                            display->ShowSdImage("E:/16.png");
                                }
                                else if(cmd == 0x17)
                                {
                                            auto display = Board::GetInstance().GetDisplay();
                                            display->ShowImage();
                                            display->ShowSdImage("E:/17.png");
                                }
                                else if(cmd == 0x18)
                                {
                                            auto display = Board::GetInstance().GetDisplay();
                                            display->ShowImage();
                                            display->ShowSdImage("E:/18.png");
                                }
                                else if(cmd == 0x19)
                                {
                                            auto display = Board::GetInstance().GetDisplay();
                                            display->ShowImage();
                                            display->ShowSdImage("E:/19.jpg");
                                }
                                else if(cmd == 0xbb)  //清图
                                {
                                            auto display = Board::GetInstance().GetDisplay();
                                            display->HideImage();
                                }
                                else if(cmd == 0xcc)
                                {
                                            ESP_LOGI(TAG, "没有中文字符数据");
                                            auto display = Board::GetInstance().GetDisplay();
                                            display->Hide(); //清文字
                                }
                            }

                            // 调用回调函数（如果已设置）
                            if (receive_callback_)
                            {
                                //printf("receive_callback_ is not null\n");
                                std::vector<uint8_t> frameData(frame_buffer_, frame_buffer_ + frame_buffer_index_ - 1);
                                receive_callback_(frameData); // 传递帧数据
                            }
                        lendflag = 0; // 重置标志
                    }
                }
                else
                {
                    ESP_LOGE(TAG, "CRC校验失败! 接收: 0x%02X, 计算: 0x%02X",
                             receivedCRC, calculatedCRC);
                }
            }
            else
            {
                ESP_LOGE(TAG, "帧太短，无法包含有效数据和CRC");
            }

            ESP_LOGD(TAG, "帧接收完成, %d 字节", frame_buffer_index_);
            frame_buffer_index_ = 0;
            //printf("6666666666\n");
        }
        else
        {
            // 继续接收数据
            if (frame_buffer_index_ < BUF_SIZE - 1)
            {
                frame_buffer_[frame_buffer_index_++] = byte;
            }
            else
            {
                // 缓冲区溢出
                ESP_LOGE(TAG, "帧缓冲区溢出!");
                resetFrameParser();
            }
        }
        break;

    default:
        // 未知状态，重置解析器
        resetFrameParser();
        break;
    }
}

// 重置帧解析器
void UartSender::resetFrameParser()
{
    frame_state_ = FrameState::IDLE;
    frame_buffer_index_ = 0;
}
