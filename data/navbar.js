// navbar.js - Shared navigation component for ESP32 Monitor

document.addEventListener('DOMContentLoaded', function() {
    // Function to create and insert the navbar
    function createNavbar() {
      // Skip creating navbar if accessed from 192.168.4.1 (this is the adress for the AP)
      if (window.location.hostname === '192.168.4.1') {
        return;
      }
      
      // Get current page filename
      const currentPage = window.location.pathname.split('/').pop() || 'index.html';
      
      // Create navbar HTML
      const navbarHTML = `
        <nav class="navbar navbar-dark bg-primary mb-4">
          <div class="container-fluid">
            <a class="navbar-brand" href="#">ESP32 Monitor</a>
            <div class="dropdown">
              <button class="navbar-toggler" type="button" data-bs-toggle="dropdown" aria-expanded="false">
                <span class="navbar-toggler-icon"></span>
              </button>
              <ul class="dropdown-menu dropdown-menu-end">
                <li><a class="dropdown-item ${currentPage === 'index.html' ? 'active' : ''}" href="index.html">Dashboard</a></li>
                <li><a class="dropdown-item ${currentPage === 'wifi_config.html' ? 'active' : ''}" href="wifi_config.html">WiFi Configuration</a></li>
                <li><a class="dropdown-item ${currentPage === 'reset_wifi.html' ? 'active' : ''}" href="reset_wifi.html">Reset WiFi</a></li>
              </ul>
            </div>
          </div>
        </nav>
      `;
      
      // Insert navbar at the beginning of body
      const bodyElement = document.body;
      const tempDiv = document.createElement('div');
      tempDiv.innerHTML = navbarHTML;
      bodyElement.insertBefore(tempDiv.firstElementChild, bodyElement.firstChild);
    }
    
    // Initialize the navbar
    createNavbar();
  });