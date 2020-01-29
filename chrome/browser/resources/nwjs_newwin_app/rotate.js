var arms;
var shadow;

var cx1, cy1;
var cx2, cy2;

function rotate(angle) {
  arms.setAttribute("transform", `rotate(${angle} ${cx1} ${cy1})`);
  shadow.setAttribute("transform", `rotate(${angle} ${cx2} ${cy2})`);
}

function animate({timing, duration}) {

  let start = performance.now();

  requestAnimationFrame(function animate(time) {
    // timeFraction goes from 0 to 1
    let timeFraction = (time - start) / duration;
    if (timeFraction > 1) timeFraction = 1;

    // calculate the current animation state
    let progress = timing(timeFraction);

    rotate(360 * progress); // draw it

    if (timeFraction < 1) {
      requestAnimationFrame(animate);
    }

  });
}

var logo = document.getElementById("logo");
logo.addEventListener("load",function () {
  var svgDoc = logo.contentDocument;
  arms = svgDoc.getElementById('g2902');
  shadow = svgDoc.getElementById('g2865');
  var coord = arms.getBBox();
  cx1 = coord.x + (coord.width / 2);
  cy1 = coord.y + (coord.height / 2);
  coord = shadow.getBBox();
  cx2 = coord.x + (coord.width / 2);
  cy2 = coord.y + (coord.height / 2);
  animate({duration: 3000, timing: easeOutElastic});
  svgDoc.addEventListener("click", ()=>{
    animate({duration: 3000, timing: easeOutElastic});
  });
});

function easeOutElastic(t) {
    var p = 0.3;
    return Math.pow(2,-10*t) * Math.sin((t-p/4)*(2*Math.PI)/p) + 1;
}
