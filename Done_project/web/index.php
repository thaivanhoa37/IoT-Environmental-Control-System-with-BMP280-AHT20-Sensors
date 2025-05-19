<?php
header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: GET, POST');
header('Access-Control-Allow-Headers: Content-Type');

function fetch_initial_data() {
    $servername = "localhost";
    $username = "root";
    $password = "1";
    $dbname = "environment_data";

    $conn = new mysqli($servername, $username, $password, $dbname);
    if ($conn->connect_error) {
        return null;
    }

    $time_range = 24;
    $sql = "SELECT timestamp, temperature, humidity, pressure, altitude 
            FROM sensor_data 
            WHERE timestamp >= NOW() - INTERVAL ? HOUR 
            ORDER BY timestamp ASC";
            
    $stmt = $conn->prepare($sql);
    $stmt->bind_param("i", $time_range);
    $stmt->execute();
    $result = $stmt->get_result();

    $data = [
        'timestamps' => [],
        'temperatures' => [],
        'humidities' => [],
        'pressures' => [],
        'altitudes' => []
    ];

    while($row = $result->fetch_assoc()) {
        $data['timestamps'][] = $row['timestamp'];
        $data['temperatures'][] = floatval($row['temperature']);
        $data['humidities'][] = floatval($row['humidity']);
        $data['pressures'][] = floatval($row['pressure']);
        $data['altitudes'][] = floatval($row['altitude']);
    }

    $stmt->close();
    $conn->close();
    return $data;
}

if (isset($_GET['action']) && $_GET['action'] === 'fetchData') {
    header('Content-Type: application/json');

    $servername = "localhost";
    $username = "root";
    $password = "1";
    $dbname = "environment_data";

    $conn = new mysqli($servername, $username, $password, $dbname);
    if ($conn->connect_error) {
        die(json_encode(['error' => 'Kết nối thất bại: ' . $conn->connect_error]));
    }

    $time_range = isset($_GET['time_range']) ? intval($_GET['time_range']) : 24;
    
    $sql = "SELECT timestamp, temperature, humidity, pressure, altitude FROM sensor_data WHERE timestamp >= NOW() - INTERVAL ? HOUR ORDER BY timestamp ASC";
    $stmt = $conn->prepare($sql);
    $stmt->bind_param("i", $time_range);
    $stmt->execute();
    $result = $stmt->get_result();

    $chart_data = [
        'timestamps' => [],
        'temperatures' => [],
        'humidities' => [],
        'pressures' => [],
        'altitudes' => []
    ];

    if ($result->num_rows > 0) {
        while($row = $result->fetch_assoc()) {
            $chart_data['timestamps'][] = $row["timestamp"];
            $chart_data['temperatures'][] = floatval($row["temperature"]);
            $chart_data['humidities'][] = floatval($row["humidity"]);
            $chart_data['pressures'][] = floatval($row["pressure"]);
            $chart_data['altitudes'][] = floatval($row["altitude"]);
        }
    }

    $stmt->close();

    $sql_latest = "SELECT timestamp, temperature, humidity, pressure, altitude FROM sensor_data ORDER BY timestamp DESC LIMIT 1";
    $result_latest = $conn->query($sql_latest);
    $latest_data = $result_latest->fetch_assoc();

    $conn->close();

    echo json_encode([
        'chart_data' => $chart_data,
        'latest_data' => $latest_data
    ]);
    exit;
}

$servername = "localhost";
$username = "root";
$password = "1";
$dbname = "environment_data";

$conn = new mysqli($servername, $username, $password, $dbname);
if ($conn->connect_error) {
    die("Kết nối thất bại: " . $conn->connect_error);
}

$sql = "SELECT timestamp, temperature, humidity, pressure, altitude FROM sensor_data ORDER BY timestamp DESC LIMIT 1";
$result = $conn->query($sql);
$latest = $result->fetch_assoc();

if (!$latest) {
    $latest = [
        'timestamp' => date('Y-m-d H:i:s'),
        'temperature' => 0,
        'humidity' => 0,
        'pressure' => 0,
        'altitude' => 0
    ];
}

$conn->close();
?>
<!DOCTYPE html>
<html lang="vi">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Hệ thống giám sát</title>
    <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.1.3/dist/css/bootstrap.min.css" rel="stylesheet">
    <link rel="stylesheet" href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0/css/all.min.css">
    <style>
        :root {
            --primary: #4e73df;
            --success: #1cc88a;
            --warning: #f6c23e;
            --danger: #e74a3b;
            --info: #36b9cc;
            --dark: #5a5c69;
        }

        body {
            background-color: #f8f9fc;
            font-family: 'Segoe UI', sans-serif;
        }

        .topbar {
            background: linear-gradient(to right, var(--primary), #224abe);
            padding: 1rem;
            color: white;
            margin-bottom: 2rem;
            border-radius: 0 0 15px 15px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }

        .status-bar {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-top: 0.5rem;
            font-size: 0.9rem;
            opacity: 0.9;
        }

        .connection-status .fa-circle {
            font-size: 0.8rem;
            margin-right: 0.5rem;
        }

        .connection-status.connected .fa-circle {
            color: var(--success);
        }

        .connection-status.disconnected .fa-circle {
            color: var(--danger);
        }

        .last-update {
            text-align: right;
        }

        .card {
            border: none;
            border-radius: 15px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
            margin-bottom: 1.5rem;
            transition: transform 0.2s;
        }

        .card:hover {
            transform: translateY(-5px);
        }

        .sensor-card {
            color: white;
            padding: 1.5rem;
        }

        .sensor-card.temperature {
            background: linear-gradient(45deg, #4e73df, #224abe);
        }

        .sensor-card.humidity {
            background: linear-gradient(45deg, #36b9cc, #1a8eaa);
        }

        .sensor-card.pressure {
            background: linear-gradient(45deg, #1cc88a, #13855c);
        }

        .sensor-card.altitude {
            background: linear-gradient(45deg, #f6c23e, #dda20a);
        }

        .sensor-value {
            font-size: 2.5rem;
            font-weight: bold;
            margin: 1rem 0;
        }

        .sensor-icon {
            font-size: 2rem;
            opacity: 0.8;
        }

        .control-btn {
            border: none;
            border-radius: 10px;
            padding: 1rem;
            font-size: 1.1rem;
            font-weight: 600;
            width: 100%;
            transition: all 0.3s;
            position: relative;
            overflow: hidden;
        }

        .control-btn::after {
            content: '';
            position: absolute;
            top: 0;
            left: 0;
            width: 100%;
            height: 100%;
            background: rgba(255,255,255,0.1);
            opacity: 0;
            transition: opacity 0.3s;
        }

        .control-btn:hover::after {
            opacity: 1;
        }

        .control-btn[data-state="ON"] {
            background-color: var(--success);
            color: white;
        }

        .control-btn[data-state="OFF"] {
            background-color: var(--danger);
            color: white;
        }

        .chart-container {
            background: white;
            padding: 1rem;
            border-radius: 15px;
            height: 500px;
            margin-bottom: 1.5rem;
        }

        .timer-settings {
            margin-top: 1rem;
            padding: 1rem;
            background: #f8f9fc;
            border-radius: 10px;
        }

        .timer-input {
            width: 100%;
            padding: 0.5rem;
            border: 1px solid #d1d3e2;
            border-radius: 5px;
            margin-bottom: 0.5rem;
        }

        .timer-btn {
            background: var(--primary);
            color: white;
            border: none;
            border-radius: 5px;
            padding: 0.5rem 1rem;
            cursor: pointer;
            transition: background 0.3s;
        }

        .timer-btn:hover {
            background: #224abe;
        }

        .timer-status {
            margin-top: 0.5rem;
            font-size: 0.9rem;
            color: var(--dark);
        }

        @media (max-width: 768px) {
            .status-bar {
                flex-direction: column;
                align-items: flex-start;
            }
            .last-update {
                text-align: left;
                margin-top: 0.5rem;
            }
            .control-btn {
                margin-bottom: 1rem;
            }
        }
    </style>
    <script src="https://cdn.jsdelivr.net/npm/moment@2.29.1/moment.min.js"></script>
    <script src="https://cdn.jsdelivr.net/npm/chart.js@3.7.0/dist/chart.min.js"></script>
    <script src="https://cdn.jsdelivr.net/npm/chartjs-adapter-moment@1.0.0/dist/chartjs-adapter-moment.min.js"></script>
    <script src="https://cdn.jsdelivr.net/npm/chartjs-plugin-zoom@1.2.1"></script>
    <script>
    </script>
</head>
<body>
    <div class="topbar">
        <div class="container">
            <h1 class="h3 mb-0">Hệ thống giám sát và điều khiển</h1>
            <div class="status-bar">
                <div class="connection-status" id="connectionStatus">
                    <i class="fas fa-circle"></i>
                    <span>Đang kết nối...</span>
                </div>
                <div class="last-update">
                    Cập nhật lúc: <span id="lastUpdate">--:--:--</span>
                </div>
            </div>
        </div>
    </div>

    <div class="container">
        <div class="row mb-4">
            <div class="col-md-3">
                <div class="card sensor-card temperature">
                    <div class="d-flex justify-content-between align-items-center">
                        <div>
                            <h4>Nhiệt độ</h4>
                            <div class="sensor-value">
                                <span id="currentTemp"><?php echo number_format($latest['temperature'], 1); ?></span>°C
                            </div>
                        </div>
                        <i class="fas fa-temperature-high sensor-icon"></i>
                    </div>
                </div>
            </div>
            <div class="col-md-3">
                <div class="card sensor-card humidity">
                    <div class="d-flex justify-content-between align-items-center">
                        <div>
                            <h4>Độ ẩm</h4>
                            <div class="sensor-value">
                                <span id="currentHumidity"><?php echo number_format($latest['humidity'], 1); ?></span>%
                            </div>
                        </div>
                        <i class="fas fa-tint sensor-icon"></i>
                    </div>
                </div>
            </div>
            <div class="col-md-3">
                <div class="card sensor-card pressure">
                    <div class="d-flex justify-content-between align-items-center">
                        <div>
                            <h4>Áp suất</h4>
                            <div class="sensor-value">
                                <span id="currentPressure"><?php echo number_format($latest['pressure'], 1); ?></span>hPa
                            </div>
                        </div>
                        <i class="fas fa-tachometer-alt sensor-icon"></i>
                    </div>
                </div>
            </div>
            <div class="col-md-3">
                <div class="card sensor-card altitude">
                    <div class="d-flex justify-content-between align-items-center">
                        <div>
                            <h4>Độ cao</h4>
                            <div class="sensor-value">
                                <span id="currentAltitude"><?php echo number_format($latest['altitude'], 1); ?></span>m
                            </div>
                        </div>
                        <i class="fas fa-mountain sensor-icon"></i>
                    </div>
                </div>
            </div>
        </div>

        <div class="card mb-4">
            <div class="card-header py-3">
                <h6 class="m-0 font-weight-bold text-primary">Điều khiển thiết bị</h6>
            </div>
            <div class="card-body">
                <div class="row">
                    <div class="col-md-4">
                        <div>
                            <button id="modeBtn" onclick="toggleDevice('mode')" 
                                    data-state="OFF" class="control-btn">
                                <i class="fas fa-sync-alt me-2"></i>
                                Chế độ tự động: TẮT
                            </button>
                            <div class="timer-settings">
                                <h6>Hẹn giờ</h6>
                                <input type="datetime-local" id="modeTimer" class="timer-input">
                                <button onclick="setTimer('mode')" class="timer-btn">Đặt hẹn giờ</button>
                                <div id="modeTimerStatus" class="timer-status"></div>
                            </div>
                        </div>
                    </div>
                    <div class="col-md-4">
                        <div>
                            <button id="heaterBtn" onclick="toggleDevice('heater')" 
                                    data-state="OFF" class="control-btn">
                                <i class="fas fa-fan me-2"></i>
                                Thiết bị 1: TẮT
                            </button>
                            <div class="timer-settings">
                                <h6>Hẹn giờ</h6>
                                <input type="datetime-local" id="heaterTimer" class="timer-input">
                                <button onclick="setTimer('heater')" class="timer-btn">Đặt hẹn giờ</button>
                                <div id="heaterTimerStatus" class="timer-status"></div>
                            </div>
                        </div>
                    </div>
                    <div class="col-md-4">
                        <div>
                            <button id="humidifierBtn" onclick="toggleDevice('humidifier')" 
                                    data-state="OFF" class="control-btn">
                                <i class="fas fa-cloud-rain me-2"></i>
                                Thiết bị 2: TẮT
                            </button>
                            <div class="timer-settings">
                                <h6>Hẹn giờ</h6>
                                <input type="datetime-local" id="humidifierTimer" class="timer-input">
                                <button onclick="setTimer('humidifier')" class="timer-btn">Đặt hẹn giờ</button>
                                <div id="humidifierTimerStatus" class="timer-status"></div>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
        </div>

        <div class="row">
            <div class="col-md-6">
                <div class="card">
                    <div class="card-header d-flex justify-content-between align-items-center">
                        <h6 class="font-weight-bold text-primary mb-0">Biểu đồ nhiệt độ</h6>
                        <div class="d-flex align-items-center gap-2">
                            <select class="form-select form-select-sm w-auto" onchange="changeTimeRange(this.value)">
                                <option value="1">1 giờ</option>
                                <option value="3">3 giờ</option>
                                <option value="6">6 giờ</option>
                                <option value="12">12 giờ</option>
                                <option value="24" selected>24 giờ</option>
                                <option value="48">48 giờ</option>
                                <option value="72">72 giờ</option>
                            </select>
                            <div>
                                <button class="btn btn-sm btn-outline-primary" onclick="zoomChart('tempChart', 'in')">
                                    <i class="fas fa-search-plus"></i>
                                </button>
                                <button class="btn btn-sm btn-outline-primary" onclick="zoomChart('tempChart', 'out')">
                                    <i class="fas fa-search-minus"></i>
                                </button>
                            </div>
                        </div>
                    </div>
                    <div class="card-body chart-container">
                        <canvas id="tempChart"></canvas>
                    </div>
                </div>
            </div>
            <div class="col-md-6">
                <div class="card">
                    <div class="card-header d-flex justify-content-between align-items-center">
                        <h6 class="font-weight-bold text-primary mb-0">Biểu đồ độ ẩm</h6>
                        <div class="d-flex align-items-center gap-2">
                            <select class="form-select form-select-sm w-auto" onchange="changeTimeRange(this.value)">
                                <option value="1">1 giờ</option>
                                <option value="3">3 giờ</option>
                                <option value="6">6 giờ</option>
                                <option value="12">12 giờ</option>
                                <option value="24" selected>24 giờ</option>
                                <option value="48">48 giờ</option>
                                <option value="72">72 giờ</option>
                            </select>
                            <div>
                                <button class="btn btn-sm btn-outline-primary" onclick="zoomChart('humidityChart', 'in')">
                                    <i class="fas fa-search-plus"></i>
                                </button>
                                <button class="btn btn-sm btn-outline-primary" onclick="zoomChart('humidityChart', 'out')">
                                    <i class="fas fa-search-minus"></i>
                                </button>
                            </div>
                        </div>
                    </div>
                    <div class="card-body chart-container">
                        <canvas id="humidityChart"></canvas>
                    </div>
                </div>
            </div>
        </div>

        <div class="row">
            <div class="col-md-6">
                <div class="card">
                    <div class="card-header d-flex justify-content-between align-items-center">
                        <h6 class="font-weight-bold text-primary mb-0">Biểu đồ áp suất</h6>
                        <div class="d-flex align-items-center gap-2">
                            <select class="form-select form-select-sm w-auto" onchange="changeTimeRange(this.value)">
                                <option value="1">1 giờ</option>
                                <option value="3">3 giờ</option>
                                <option value="6">6 giờ</option>
                                <option value="12">12 giờ</option>
                                <option value="24" selected>24 giờ</option>
                                <option value="48">48 giờ</option>
                                <option value="72">72 giờ</option>
                            </select>
                            <div>
                                <button class="btn btn-sm btn-outline-primary" onclick="zoomChart('pressureChart', 'in')">
                                    <i class="fas fa-search-plus"></i>
                                </button>
                                <button class="btn btn-sm btn-outline-primary" onclick="zoomChart('pressureChart', 'out')">
                                    <i class="fas fa-search-minus"></i>
                                </button>
                            </div>
                        </div>
                    </div>
                    <div class="card-body chart-container">
                        <canvas id="pressureChart"></canvas>
                    </div>
                </div>
            </div>
            <div class="col-md-6">
                <div class="card">
                    <div class="card-header d-flex justify-content-between align-items-center">
                        <h6 class="font-weight-bold text-primary mb-0">Biểu đồ độ cao</h6>
                        <div class="d-flex align-items-center gap-2">
                            <select class="form-select form-select-sm w-auto" onchange="changeTimeRange(this.value)">
                                <option value="1">1 giờ</option>
                                <option value="3">3 giờ</option>
                                <option value="6">6 giờ</option>
                                <option value="12">12 giờ</option>
                                <option value="24" selected>24 giờ</option>
                                <option value="48">48 giờ</option>
                                <option value="72">72 giờ</option>
                            </select>
                            <div>
                                <button class="btn btn-sm btn-outline-primary" onclick="zoomChart('altitudeChart', 'in')">
                                    <i class="fas fa-search-plus"></i>
                                </button>
                                <button class="btn btn-sm btn-outline-primary" onclick="zoomChart('altitudeChart', 'out')">
                                    <i class="fas fa-search-minus"></i>
                                </button>
                            </div>
                        </div>
                    </div>
                    <div class="card-body chart-container">
                        <canvas id="altitudeChart"></canvas>
                    </div>
                </div>
            </div>
        </div>
    </div>

    <script>
        const hostname = window.location.hostname;
        let tempChart, humidityChart, pressureChart, altitudeChart;
        let ws;

        function updateConnectionStatus(connected, message = null) {
            const status = document.getElementById('connectionStatus');
            status.className = 'connection-status ' + (connected ? 'connected' : 'disconnected');
            status.innerHTML = `
                <i class="fas fa-circle"></i>
                <span>${message || (connected ? 'Đã kết nối' : 'Mất kết nối')}</span>
            `;
        }

        function updateLastUpdate() {
            const now = new Date();
            document.getElementById('lastUpdate').textContent = 
                now.getHours().toString().padStart(2, '0') + ':' +
                now.getMinutes().toString().padStart(2, '0') + ':' +
                now.getSeconds().toString().padStart(2, '0');
        }

<?php 
$initial_data = fetch_initial_data();
if (!$initial_data) {
    $initial_data = [
        'timestamps' => [],
        'temperatures' => [],
        'humidities' => [],
        'pressures' => [],
        'altitudes' => []
    ];
}
?>
        const initialData = <?php echo json_encode($initial_data); ?>;
        console.log('Initial data loaded:', initialData.timestamps.length, 'points');

        function initCharts() {
                function zoomChart(chartId, direction) {
                    const chart = window[chartId];
                    const range = chart.scales.x.max - chart.scales.x.min;
                    const center = (chart.scales.x.max + chart.scales.x.min) / 2;
                    const factor = direction === 'in' ? 0.5 : 2;
                    
                    const newRange = range * factor;
                    chart.options.scales.x.min = new Date(center - newRange / 2);
                    chart.options.scales.x.max = new Date(center + newRange / 2);
                    chart.update('none');
                }

                function createChartOptions(label) {
                return {
                    responsive: true,
                    maintainAspectRatio: false,
                    animation: {
                        duration: 0
                    },
                    plugins: {
                        tooltip: {
                            mode: 'index',
                            intersect: false,
                            backgroundColor: 'rgba(255,255,255,0.9)',
                            titleColor: '#666',
                            bodyColor: '#666',
                            borderColor: '#ddd',
                            borderWidth: 1
                        },
                        legend: {
                            display: true,
                            position: 'top',
                            labels: {
                                usePointStyle: true,
                                boxWidth: 6
                            }
                        },
                        zoom: {
                            pan: {
                                enabled: true,
                                mode: 'x'
                            },
                            zoom: {
                                wheel: {
                                    enabled: true
                                },
                                pinch: {
                                    enabled: true
                                },
                                mode: 'x'
                            }
                        }
                    },
                    scales: {
                        x: {
                            type: 'time',
                            display: true,
                            time: {
                                parser: 'YYYY-MM-DD HH:mm:ss',
                                tooltipFormat: 'HH:mm:ss',
                                unit: 'minute',
                                displayFormats: {
                                    minute: 'HH:mm',
                                    hour: 'HH:mm'
                                }
                            },
                            adapters: {
                                date: {
                                    locale: 'vi'
                                }
                            },
                            title: {
                                display: true,
                                text: 'Thời gian'
                            }
                        },
                        y: {
                            beginAtZero: false,
                            title: {
                                display: true,
                                text: label
                            }
                        }
                    }
                };
            }

            tempChart = createChart('tempChart', 'Nhiệt độ (°C)', '#4e73df', createChartOptions('Nhiệt độ (°C)'));
            humidityChart = createChart('humidityChart', 'Độ ẩm (%)', '#36b9cc', createChartOptions('Độ ẩm (%)'));
            pressureChart = createChart('pressureChart', 'Áp suất (hPa)', '#1cc88a', createChartOptions('Áp suất (hPa)'));
            altitudeChart = createChart('altitudeChart', 'Độ cao (m)', '#f6c23e', createChartOptions('Độ cao (m)'));

            if (initialData) {
                updateChart(tempChart, initialData.timestamps, initialData.temperatures);
                updateChart(humidityChart, initialData.timestamps, initialData.humidities);
                updateChart(pressureChart, initialData.timestamps, initialData.pressures);
                updateChart(altitudeChart, initialData.timestamps, initialData.altitudes);
            }

            // Update all charts initially
            tempChart.update();
            humidityChart.update();
            pressureChart.update();
            altitudeChart.update();
        }

        function createChart(canvasId, label, color, options) {
            const defaultDataset = {
                label: label,
                data: [],
                borderColor: color,
                backgroundColor: 'transparent',
                fill: false,
                cubicInterpolationMode: 'monotone',
                tension: 0.5,
                borderWidth: 3,
                pointRadius: 2,
                pointHoverRadius: 6,
                pointBackgroundColor: color,
                pointBorderColor: color,
                pointHoverBackgroundColor: '#fff',
                pointHoverBorderColor: color,
                pointHoverBorderWidth: 2
            };

            return new Chart(document.getElementById(canvasId).getContext('2d'), {
                type: 'line',
                data: {
                    datasets: [defaultDataset]
                },
                options: options
            });
        }

        let timeRange = 24; // Default time range in hours
        
        function changeTimeRange(hours) {
            timeRange = parseInt(hours);
            updateData();
        }

        function updateData() {
            if (!ws || ws.readyState !== WebSocket.OPEN) return;
            fetch(`?action=fetchData&time_range=${timeRange}`)
                .then(response => response.json())
                .then(data => {
                    if (!data) return;

                    const latest = data.latest_data;
                    const chartData = data.chart_data;

                    // Update current values with animation
                    animateValue('currentTemp', parseFloat(document.getElementById('currentTemp').textContent), latest.temperature, 1000);
                    animateValue('currentHumidity', parseFloat(document.getElementById('currentHumidity').textContent), latest.humidity, 1000);
                    animateValue('currentPressure', parseFloat(document.getElementById('currentPressure').textContent), latest.pressure, 1000);
                    animateValue('currentAltitude', parseFloat(document.getElementById('currentAltitude').textContent), latest.altitude, 1000);

                    // Update charts
                    updateChart(tempChart, chartData.timestamps, chartData.temperatures);
                    updateChart(humidityChart, chartData.timestamps, chartData.humidities);
                    updateChart(pressureChart, chartData.timestamps, chartData.pressures);
                    updateChart(altitudeChart, chartData.timestamps, chartData.altitudes);

                    // Update last update time
                    updateLastUpdate();
                })
                .catch(error => {
                    console.error('Error:', error);
                    updateConnectionStatus(false, 'Lỗi kết nối dữ liệu');
                });
        }

        function animateValue(id, start, end, duration) {
            if (isNaN(start) || isNaN(end)) return;
            
            const obj = document.getElementById(id);
            const range = end - start;
            const minTimer = 50;
            const stepTime = Math.abs(Math.floor(duration / range));
            const timerDelay = Math.max(stepTime, minTimer);
            const startTime = new Date().getTime();

            function updateTimer() {
                const now = new Date().getTime();
                const remaining = Math.max((startTime + duration) - now, 0);
                const value = Math.round((end - ((remaining / duration) * range)) * 10) / 10;
                obj.textContent = value.toFixed(1);
                if (value === end) return;
                requestAnimationFrame(updateTimer);
            }
            requestAnimationFrame(updateTimer);
        }

        function updateChart(chart, timestamps, values) {
            if (!timestamps || !values || timestamps.length === 0) return;

            try {
                // Convert timestamps to Date objects and parse values
                const chartData = timestamps.map((timestamp, index) => {
                    const value = parseFloat(values[index]);
                    return !isNaN(value) ? {
                        x: moment(timestamp).toDate(),
                        y: value
                    } : null;
                }).filter(point => point !== null);

                // Keep existing options but update data
                const existingDataset = chart.data.datasets[0];
                chart.data.datasets = [{
                    ...existingDataset,
                    data: chartData
                }];

                // Update with proper animation config
                chart.update({
                    duration: 0,
                    easing: 'linear',
                    preservation: true
                });
                
                if (chartData.length > 0) {
                    console.log(`Updated chart with ${chartData.length} points from ${chartData[0]?.x} to ${chartData[chartData.length-1]?.x}`);
                }
            } catch (error) {
                console.error('Error updating chart:', error);
            }
        }

        function toggleDevice(device) {
            if (!ws || ws.readyState !== WebSocket.OPEN) {
                updateConnectionStatus(false, 'Mất kết nối');
                return;
            }

            const btn = document.getElementById(device + 'Btn');
            const currentState = btn.getAttribute('data-state');
            const newState = currentState === 'OFF' ? 'ON' : 'OFF';
            
            const message = {
                type: device,
                state: newState
            };

            // Update button appearance immediately for better UX
            btn.setAttribute('data-state', newState);
            let label, icon;
            switch(device) {
                case 'mode':
                    label = 'Chế độ tự động';
                    icon = 'fa-sync-alt';
                    break;
                case 'heater':
                    label = 'Thiết bị 1';
                    icon = 'fa-fan';
                    break;
                case 'humidifier':
                    label = 'Thiết bị 2';
                    icon = 'fa-cloud-rain';
                    break;
            }
            btn.innerHTML = `<i class="fas ${icon} me-2"></i>${label}: ${newState === 'ON' ? 'BẬT' : 'TẮT'}`;
            btn.className = `control-btn ${newState === 'ON' ? 'on' : 'off'}`;

            console.log('Sending:', message);
            ws.send(JSON.stringify(message));
        }

        function connectWebSocket() {
            if (ws) {
                ws.close();
            }

            ws = new WebSocket(`ws://${hostname}:1880/ws/node/state`);
            

            ws.onopen = function() {
                console.log('WebSocket Connected');
                updateConnectionStatus(true);
                updateData();
                
                // Subscribe to MQTT state topics via Node-RED
                console.log('Subscribing to state topics...');
            };

            ws.onclose = function() {
                console.log('WebSocket Disconnected');
                updateConnectionStatus(false);
                setTimeout(connectWebSocket, 5000);
            };

            ws.onerror = function(err) {
                console.error('WebSocket Error:', err);
                updateConnectionStatus(false, 'Lỗi kết nối');
            };

            ws.onmessage = function(evt) {
                try {
                    const data = JSON.parse(evt.data);
                    if (data.type && data.state) {
                        const device = data.type;
                        const state = data.state;
                        const btn = document.getElementById(device + 'Btn');
                        if (btn) {
                            btn.setAttribute('data-state', state);
                            let label, icon;
                            switch(device) {
                                case 'mode':
                                    label = 'Chế độ tự động';
                                    icon = 'fa-sync-alt';
                                    break;
                                case 'heater':
                                    label = 'Thiết bị 1';
                                    icon = 'fa-fan';
                                    break;
                                case 'humidifier':
                                    label = 'Thiết bị 2';
                                    icon = 'fa-cloud-rain';
                                    break;
                            }
                            btn.innerHTML = `<i class="fas ${icon} me-2"></i>${label}: ${state === 'ON' ? 'BẬT' : 'TẮT'}`;
                            btn.className = `control-btn ${state === 'ON' ? 'on' : 'off'}`;
                        }
                    }
                } catch (e) {
                    console.error('Error handling message:', e);
                }
            };
        }

        // Initialize
        document.addEventListener('DOMContentLoaded', function() {
            initCharts();
            connectWebSocket();
            
            // Update data every 5 seconds if connected
            setInterval(function() {
                if (ws && ws.readyState === WebSocket.OPEN) {
                    updateData();
                }
            }, 5000);
        });

        let timers = {};

        function setTimer(device) {
            const timerInput = document.getElementById(device + 'Timer');
            const statusDiv = document.getElementById(device + 'TimerStatus');
            const targetTime = new Date(timerInput.value).getTime();
            const now = new Date().getTime();

            if (targetTime <= now) {
                statusDiv.textContent = 'Vui lòng chọn thời gian trong tương lai';
                return;
            }

            // Clear existing timer if any
            if (timers[device]) {
                clearTimeout(timers[device]);
            }

            // Set new timer
            const delay = targetTime - now;
            timers[device] = setTimeout(() => {
                toggleDevice(device);
                statusDiv.textContent = 'Hẹn giờ đã thực hiện';
                timerInput.value = '';
            }, delay);

            // Update status
            const minutes = Math.floor(delay / 60000);
            const hours = Math.floor(minutes / 60);
            const remainingMinutes = minutes % 60;
            statusDiv.textContent = `Hẹn giờ: ${hours} giờ ${remainingMinutes} phút`;
        }
    </script>
</body>
</html>
