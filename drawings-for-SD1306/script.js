const grid = document.getElementById('circleGrid');
const downloadBtn = document.getElementById('downloadBtn');

let isMouseDown = false;
const totalCircles = 128 * 64;

// Create the grid
for (let i = 0; i < totalCircles; i++) {
    const circle = document.createElement('div');
    circle.classList.add('circle');
    circle.dataset.active = "false";
    circle.style.backgroundColor = '#191919';

    circle.addEventListener('click', () => toggleCircle(circle));
    circle.addEventListener('mouseover', () => {
        if (isMouseDown) toggleCircle(circle);
    });

    grid.appendChild(circle);
}

// Mouse tracking for drag coloring
document.addEventListener('mousedown', (e) => {
    if (!downloadBtn.contains(e.target)) isMouseDown = true;
});
document.addEventListener('mouseup', () => {
    isMouseDown = false;
});

// Toggle color: black ⇄ white
function toggleCircle(circle) {
    const active = circle.dataset.active === "true";

    if (!active) {
        circle.style.backgroundColor = '#f3f3f3';
        circle.dataset.active = "true";
    } else {
        circle.style.backgroundColor = '#191919';
        circle.dataset.active = "false";
    }
}

// Download as SVG
downloadBtn.addEventListener('click', () => {
    const circles = document.querySelectorAll('.circle');
    const columns = 128;
    const rows = 64;
    const circleSize = 8;
    const padding = 0;

    const width = columns * circleSize + padding * 2;
    const height = rows * circleSize + padding * 2;

    let svgContent = `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}">`;

    circles.forEach((circle, index) => {
        const col = index % columns;
        const row = Math.floor(index / columns);
        const x = col * circleSize + padding;
        const y = row * circleSize + padding;
        const fillColor = circle.dataset.active === "true" ? '#f3f3f3' : '#191919';

        svgContent += `<rect x="${x}" y="${y}" width="${circleSize}" height="${circleSize}" fill="${fillColor}" />`;
    });

    svgContent += `</svg>`;

    const blob = new Blob([svgContent], { type: "image/svg+xml;charset=utf-8" });
    const url = URL.createObjectURL(blob);

    const link = document.createElement('a');
    link.href = url;
    link.download = 'play_full_canvas.svg';
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);

    URL.revokeObjectURL(url);
});

