function cp2cbFallback(txt) {
  var txtArea = document.createElement("textarea");
  txtArea.value = txt
  document.body.appendChild(txtArea);
  txtArea.focus();
  txtArea.select();
  try {
    var ret = document.execCommand("copy");
    var msg = ret ? "success" : "failure";
    console.log("[FALLBACK] Copy to clipboard: " + msg);
  } catch (err) {
    console.error("[FALLBACK] Unable to copy: ", err);
  }
  document.body.removeChild(txtArea);
}

function cp2cbAsync(txt) {
  navigator.clipboard.writeText(txt).then(
    function() {
      console.log("[ASYNC] Copying to clipboard was successful!");
    },
    function(err) {
      console.error("[ASYNC] Unable to copy: ", err);
      return
    }
  );
}

var cp2cbBtn = document.querySelector(".js-cp2cb-btn");

cp2cbBtn.addEventListener("click", function(event) {
  var cp2cbTxt = document.querySelector('.js-cp2cb-txt');
  if (navigator.clipboard) {
    cp2cbAsync(cp2cbTxt.value);
  } else {
    cp2cbFallback(cp2cbTxt.value);
  }
  cp2cbBtn.title = "Link copied to clipboard!";
});
