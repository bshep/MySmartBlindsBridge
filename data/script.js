var hostnameForm = document.getElementById("myHostnameForm");

hostnameForm.onsubmit = function (event) {
  document.location.search =
    "cmd=hostname&value=" + document.getElementById("hostname").value;
  event.preventDefault();
};

var hostNameButton = document.getElementById("myHostNameButton");

hostNameButton.onclick = function () {
  document.location.search =
    "cmd=hostname&value=" + document.getElementById("hostname").value;
};

var brokerAddressForm = document.getElementById("myBrokerAddressForm");

brokerAddressForm.onsubmit = function (event) {
  document.location.search =
    "cmd=brokerAddress&value=" + document.getElementById("brokerAddress").value;
  event.preventDefault();
};

var brokerAddressButton = document.getElementById("myBrokerAddressButton");

brokerAddressButton.onclick = function () {
  document.location.search =
    "cmd=brokerAddress&value=" + document.getElementById("brokerAddress").value;
};


var myBlindsConfigButton = document.getElementById("myBlindsConfigButton");
myBlindsConfigButton.onclick = function () {
  document.location.search =
    "cmd=blindsConfig&value=" + encodeURIComponent(document.getElementById("myBlindsConfig").value);
};


