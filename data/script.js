// Configuration
const MAX_DATA_POINTS = 50;
let tempData = [];
let chart;
let socket;

// DOM elements
const currentTempElement = document.getElementById('current-temp');
const avgTempElement = document.getElementById('avg-temp');
const lastUpdateElement = document.getElementById('last-update');
const wsStatusElement = document.getElementById('ws-status');

// Initialize chart
function initChart() {
    const ctx = document.getElementById('tempChart').getContext('2d');
    
    chart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'Temperature (°C)',
                data: [],
                borderColor: 'rgb(75, 192, 192)',
                backgroundColor: 'rgba(75, 192, 192, 0.2)',
                borderWidth: 2,
                tension: 0.2,
                fill: true
            }]
        },
        options: {
            responsive: true,
            maintainAspectRatio: false,
            scales: {
                x: {
                    title: {
                        display: true,
                        text: 'Time'
                    }
                },
                y: {
                    title: {
                        display: true,
                        text: 'Temperature (°C)'
                    },
                    beginAtZero: false
                }
            },
            plugins: {
                legend: {
                    position: 'top',
                },
                tooltip: {
                    callbacks: {
                        title: function(tooltipItem) {
                            return tempData[tooltipItem[0].dataIndex].timestamp;
                        }
                    }
                }
            }
        }
    });
}

// Format timestamp for display
function formatTimestamp(timestamp) {
    const date = new Date(timestamp);
    return date.toLocaleString();
}

// Connect to WebSocket
function connectWebSocket() {
    // Get the current host
    const host = window.location.hostname;
    const wsUrl = `ws://${host}/ws`;
    
    socket = new WebSocket(wsUrl);
    
    socket.onopen = function() {
        console.log('WebSocket connected');
        wsStatusElement.textContent = 'Connected';
        wsStatusElement.className = 'connected';
    };
    
    socket.onclose = function() {
        console.log('WebSocket disconnected');
        wsStatusElement.textContent = 'Disconnected';
        wsStatusElement.className = 'disconnected';
        
        // Try to reconnect after 5 seconds
        setTimeout(connectWebSocket, 5000);
    };
    
    socket.onerror = function(error) {
        console.error('WebSocket error:', error);
        wsStatusElement.textContent = 'Error';
        wsStatusElement.className = 'disconnected';
    };
    
    socket.onmessage = function(event) {
        try {
            const data = JSON.parse(event.data);
            
            if (data.temperature && data.timestamp) {
                updateData(data);
            }
        } catch (error) {
            console.error('Error parsing WebSocket message:', error);
        }
    };
}

// Update data with new temperature readings
function updateData(data) {
    // Add the new data point
    tempData.push(data);
    
    // Limit the number of data points
    if (tempData.length > MAX_DATA_POINTS) {
        tempData.shift();
    }
    
    // Update chart
    updateChart();
    
    // Update info displays
    updateInfoDisplay();
}

// Update the chart with current data
function updateChart() {
    // Clear existing data
    chart.data.labels = [];
    chart.data.datasets[0].data = [];
    
    // Add all current data points
    tempData.forEach(data => {
        // Extract just the time part for the x-axis
        const timeOnly = new Date(data.timestamp).toLocaleTimeString();
        chart.data.labels.push(timeOnly);
        chart.data.datasets[0].data.push(data.temperature);
    });
    
    // Update the chart
    chart.update();
}

// Update information display
function updateInfoDisplay() {
    if (tempData.length > 0) {
        // Get the latest reading
        const latestData = tempData[tempData.length - 1];
        
        // Update current temperature
        currentTempElement.textContent = latestData.temperature.toFixed(2);
        
        // Calculate and update average temperature
        const sum = tempData.reduce((total, data) => total + data.temperature, 0);
        const avg = sum / tempData.length;
        avgTempElement.textContent = avg.toFixed(2);
        
        // Update timestamp
        lastUpdateElement.textContent = formatTimestamp(latestData.timestamp);
    }
}

// Load initial data from JSON file
async function loadInitialData() {
    try {
        const response = await fetch('/temperature_log.csv');
        if (!response.ok) {
            throw new Error(`HTTP error: ${response.status}`);
        }
        
        const data = await response.csv();
        
        // Process the data - limit to MAX_DATA_POINTS most recent entries
        tempData = Array.isArray(data) ? 
            data.slice(-MAX_DATA_POINTS) : 
            (data.readings ? data.readings.slice(-MAX_DATA_POINTS) : []);
        
        // Update the chart and info display
        updateChart();
        updateInfoDisplay();
        
    } catch (error) {
        console.error('Error loading initial data:', error);
    }
}

// Initialize everything when the page loads
window.addEventListener('load', function() {
    initChart();
    loadInitialData();
    connectWebSocket();
    
    // Set up periodic refresh for data (every 30 seconds as fallback)
    setInterval(loadInitialData, 30000);
});