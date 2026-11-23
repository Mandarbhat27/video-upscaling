let perfChart = null;

function initCharts() {
    const ctx = document.getElementById('perfChart').getContext('2d');
    perfChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: Array(30).fill(''),
            datasets: [
                {
                    label: 'Processing Latency (ms)',
                    data: Array(30).fill(null),
                    borderColor: '#0d6efd',
                    backgroundColor: 'rgba(13, 110, 253, 0.15)',
                    borderWidth: 2,
                    fill: true,
                    tension: 0.3,
                    yAxisID: 'y'
                },
                {
                    label: 'Frame Budget (ms)',
                    data: Array(30).fill(8.33),
                    borderColor: '#dc3545',
                    borderWidth: 1.5,
                    borderDash: [5, 5],
                    fill: false,
                    pointRadius: 0,
                    yAxisID: 'y'
                },
                {
                    label: 'Achieved FPS',
                    data: Array(30).fill(null),
                    borderColor: '#198754',
                    borderWidth: 2,
                    fill: false,
                    tension: 0.3,
                    yAxisID: 'y1'
                }
            ]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            animation: false,
            scales: {
                x: {
                    display: false
                },
                y: {
                    type: 'linear',
                    display: true,
                    position: 'left',
                    title: { display: true, text: 'Latency (ms)', color: '#8b949e' },
                    grid: { color: '#30363d' },
                    ticks: { color: '#8b949e' },
                    min: 0,
                    suggestedMax: 60
                },
                y1: {
                    type: 'linear',
                    display: true,
                    position: 'right',
                    title: { display: true, text: 'FPS', color: '#8b949e' },
                    grid: { drawOnChartArea: false },
                    ticks: { color: '#8b949e' },
                    min: 0,
                    suggestedMax: 150
                }
            },
            plugins: {
                legend: {
                    labels: { color: '#c9d1d9', boxWidth: 12, font: { size: 11 } }
                }
            }
        }
    });
}

function updateChartData(latency, budget, fps) {
    if (!perfChart) return;

    const dataLatency = perfChart.data.datasets[0].data;
    const dataBudget = perfChart.data.datasets[1].data;
    const dataFps = perfChart.data.datasets[2].data;

    dataLatency.shift();
    dataLatency.push(latency);

    dataBudget.shift();
    dataBudget.push(budget);

    dataFps.shift();
    dataFps.push(fps);

    perfChart.update();
}
