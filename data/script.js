// Initialize the chart
const chart = new Chart(document.getElementById('tempChart'), {
	type: 'line',
	data: {
	  labels: [],
	  datasets: [{
		label: 'Temperature (°C)',
		data: [],
		borderWidth: 2,
		tension: 0.2,
		fill: true,
	  }]
	},
	options: {
	  scales: {
		x: { title: { display: true, text: 'Time' } },
		y: { title: { display: true, text: '°C' } }
	  }
	}
  });
  
  // Add new data point to chart
  function addPoint({ temperature, timestamp }) {
	const t = new Date(timestamp);
	const label = t.toLocaleTimeString();
	
	chart.data.labels.push(label);
	chart.data.datasets[0].data.push(temperature);
	chart.update('none');
  }
  
  // Load CSV and seed chart
  async function loadInitial() {
	try {
	  const res = await fetch('/temperature_log.csv');
	  if (!res.ok) throw new Error(res.statusText);
	  
	  const text = await res.text();
	  const rows = text.trim().split('\n');
	  
	  rows.forEach(line => {
		const [timestamp, temperature] = line.split(',');
		addPoint({ timestamp, temperature: Number(temperature) });
	  });
	} catch (err) {
	  console.error('Failed to load initial data:', err);
	}
  }
  
  // WebSocket connection with auto-reconnect
  function connectWebSocket() {
	const ws = new WebSocket(`ws://${location.host}/ws`);
	
	ws.onopen = () => console.log('WebSocket connected');
	ws.onclose = () => setTimeout(connectWebSocket, 5000);
	ws.onerror = (err) => console.error('WebSocket error:', err);
	
	ws.onmessage = (event) => {
	  try {
		const data = JSON.parse(event.data);
		if (data.temperature != null && data.timestamp) {
		  addPoint(data);
		}
	  } catch (err) {
		console.error('Error processing message:', err);
	  }
	};
  }
  
  // Initialize on page load
  window.addEventListener('load', () => {
	loadInitial();
	connectWebSocket();
  });