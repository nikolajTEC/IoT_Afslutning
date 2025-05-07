// Initialize the chart
const chart = new Chart(document.getElementById('tempChart'), {
	type: 'line',
	data: {
	  labels: [],
	  datasets: [{
		label: 'Temperature (°C)',
		data: [],
		borderColor: 'rgba(75, 192, 192, 1)',
		backgroundColor: 'rgba(75, 192, 192, 0.2)',
		borderWidth: 2,
		tension: 0.2,
		fill: true,
	  }]
	},
	options: {
	  responsive: true,
	  animation: {
		duration: 0 // Disable animation for better performance when adding many points
	  },
	  scales: {
		x: { title: { display: true, text: 'Time' } },
		y: { title: { display: true, text: '°C' } }
	  }
	}
  });
  
  // Add new data point to chart
  function addPoint({ temperature, timestamp }) {
	try {
	  const t = new Date(timestamp);
	  const label = t.toLocaleTimeString();
	  
	  chart.data.labels.push(label);
	  chart.data.datasets[0].data.push(temperature);
	  
	  // Limit the number of points shown to prevent performance issues
	  if (chart.data.labels.length > 100) {
		chart.data.labels.shift();
		chart.data.datasets[0].data.shift();
	  }
	  
	  chart.update('none');
	} catch (err) {
	  console.error('Error adding point:', err);
	}
  }
  
  // Load CSV and seed chart
  async function loadInitial() {
	try {
	  console.log('Loading initial data...');
	  const res = await fetch('/temperature_log.csv');
	  if (!res.ok) {
		throw new Error(`Failed to fetch CSV: ${res.status} ${res.statusText}`);
	  }
	  
	  const text = await res.text();
	  console.log('CSV loaded, processing data...');
	  
	  // Skip empty lines and handle headers if present
	  const rows = text.trim().split('\n').filter(line => line.trim() !== '');
	  
	  if (rows.length === 0) {
		console.log('No data in CSV file');
		return;
	  }
	  
	  // Check if the first row might be headers
	  const firstRow = rows[0].split(',');
	  const startIndex = isNaN(parseFloat(firstRow[1])) ? 1 : 0;
	  
	  for (let i = startIndex; i < rows.length; i++) {
		const line = rows[i];
		const [timestamp, temperature] = line.split(',');
		const tempValue = parseFloat(temperature);
		
		if (!isNaN(tempValue) && timestamp) {
		  addPoint({ 
			timestamp, 
			temperature: tempValue 
		  });
		}
	  }
	  
	  console.log(`Added ${rows.length - startIndex} points from CSV`);
	} catch (err) {
	  console.error('Failed to load initial data:', err);
	}
  }
  
  // WebSocket connection with auto-reconnect
  function connectWebSocket() {
	const ws = new WebSocket(`ws://${location.host}/ws`);
	
	ws.onopen = () => console.log('WebSocket connected');
	ws.onclose = () => {
	  console.log('WebSocket disconnected, reconnecting in 5 seconds...');
	  setTimeout(connectWebSocket, 5000);
	};
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
  window.addEventListener('DOMContentLoaded', () => {
	console.log('Page loaded, initializing...');
	loadInitial().then(() => {
	  console.log('Initial data loaded, connecting WebSocket...');
	  connectWebSocket();
	});
  });