document.addEventListener('DOMContentLoaded', () => {
    loadBenchmarkData();
});

let speedupChartInstance = null;
let efficiencyChartInstance = null;

function loadBenchmarkData() {
    fetch('/api/benchmark_data')
        .then(res => res.json())
        .then(records => {
            renderTable(records);
            renderCharts(records);
        })
        .catch(err => console.error(err));
}

function renderTable(records) {
    const tbody = document.getElementById('benchmark-table-body');
    const badge = document.getElementById('record-count-badge');
    badge.innerText = records.length + ' Records';
    tbody.innerHTML = '';

    records.forEach(r => {
        const tr = document.createElement('tr');
        const speedupClass = r.speedup > 1.0 ? 'text-success fw-bold' : 'text-secondary';
        tr.innerHTML = 
            '<td><span class="badge bg-secondary">' + r.policy + '</span></td>' +
            '<td><code>' + r.schedule + '</code></td>' +
            '<td>' + r.threads + '</td>' +
            '<td>' + r.scale_factor + 'x</td>' +
            '<td>' + r.input_resolution + ' &rarr; ' + r.output_resolution + '</td>' +
            '<td class="text-info fw-bold">' + r.method + '</td>' +
            '<td>' + r.avg_latency_ms.toFixed(2) + ' / ' + r.min_latency_ms.toFixed(2) + ' ms</td>' +
            '<td class="text-light">' + r.fps.toFixed(1) + '</td>' +
            '<td class="' + speedupClass + '">' + r.speedup.toFixed(2) + 'x</td>' +
            '<td class="text-warning">' + (r.parallel_efficiency * 100).toFixed(1) + '%</td>';
        tbody.appendChild(tr);
    });
}

function renderCharts(records) {
    const threads = [1, 2, 4, 8, 12];
    const methods = ['NEAREST', 'BILINEAR', 'BICUBIC'];
    const colors = {
        'NEAREST': '#198754',
        'BILINEAR': '#0d6efd',
        'BICUBIC': '#ffc107'
    };

    const speedupDatasets = [];
    const efficiencyDatasets = [];

    methods.forEach(m => {
        const speedupData = [];
        const effData = [];

        threads.forEach(th => {
            const match = records.find(r => r.method === m && r.threads === th && (r.policy === 'OPENMP_STATIC' || r.policy === 'SERIAL'));
            if (match) {
                speedupData.push(match.speedup);
                effData.push((match.parallel_efficiency * 100).toFixed(1));
            } else {
                speedupData.push(null);
                effData.push(null);
            }
        });

        speedupDatasets.push({
            label: m,
            data: speedupData,
            borderColor: colors[m],
            backgroundColor: colors[m],
            borderWidth: 2,
            tension: 0.2
        });

        efficiencyDatasets.push({
            label: m,
            data: effData,
            borderColor: colors[m],
            backgroundColor: colors[m],
            borderWidth: 2,
            tension: 0.2
        });
    });

    // Speedup Chart
    const ctxSpeedup = document.getElementById('speedupChart').getContext('2d');
    if (speedupChartInstance) speedupChartInstance.destroy();
    speedupChartInstance = new Chart(ctxSpeedup, {
        type: 'line',
        data: {
            labels: ['1 Thread (Serial)', '2 Threads', '4 Threads', '8 Threads', '12 Threads'],
            datasets: speedupDatasets
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                y: {
                    title: { display: true, text: 'Speedup Factor (x)', color: '#8b949e' },
                    grid: { color: '#30363d' },
                    ticks: { color: '#8b949e' },
                    min: 0.5
                },
                x: {
                    grid: { color: '#30363d' },
                    ticks: { color: '#8b949e' }
                }
            },
            plugins: {
                legend: { labels: { color: '#c9d1d9' } }
            }
        }
    });

    // Efficiency Chart
    const ctxEff = document.getElementById('efficiencyChart').getContext('2d');
    if (efficiencyChartInstance) efficiencyChartInstance.destroy();
    efficiencyChartInstance = new Chart(ctxEff, {
        type: 'bar',
        data: {
            labels: ['1 Thread', '2 Threads', '4 Threads', '8 Threads', '12 Threads'],
            datasets: efficiencyDatasets
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                y: {
                    title: { display: true, text: 'Parallel Efficiency (%)', color: '#8b949e' },
                    grid: { color: '#30363d' },
                    ticks: { color: '#8b949e' },
                    min: 0,
                    max: 110
                },
                x: {
                    grid: { color: '#30363d' },
                    ticks: { color: '#8b949e' }
                }
            },
            plugins: {
                legend: { labels: { color: '#c9d1d9' } }
            }
        }
    });
}

function triggerBenchmark() {
    const btn = document.getElementById('btn-run-bench');
    btn.disabled = true;
    btn.innerHTML = '<span class="spinner-border spinner-border-sm me-1"></span> Running Tests...';

    fetch('/api/run_benchmark', { method: 'POST' })
        .then(res => res.json())
        .then(data => {
            btn.disabled = false;
            btn.innerHTML = '<i class="bi bi-arrow-repeat me-1"></i> Re-run Benchmark';
            loadBenchmarkData();
        })
        .catch(err => {
            btn.disabled = false;
            btn.innerHTML = '<i class="bi bi-arrow-repeat me-1"></i> Re-run Benchmark';
            alert('Benchmark failed: ' + err);
        });
}
