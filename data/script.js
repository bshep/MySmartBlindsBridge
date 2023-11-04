function buttonClick(command) {
    var xhr = new XMLHttpRequest();

    xhr.open("GET", command);
    xhr.send();
}