import cv2
import pytesseract
import numpy as np
from collections import deque

# Configure path to tesseract if needed (Windows)
pytesseract.pytesseract.tesseract_cmd = r'C:\Program Files\Tesseract-OCR\tesseract.exe'

def preprocess_display(frame):
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    blur = cv2.GaussianBlur(gray, (5,5), 0)
    thresh = cv2.adaptiveThreshold(blur, 255,
                                   cv2.ADAPTIVE_THRESH_GAUSSIAN_C,
                                   cv2.THRESH_BINARY_INV, 11, 2)
    return thresh

def find_display_roi(thresh):
    contours, _ = cv2.findContours(thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
    display_contour = None
    max_area = 0

    for cnt in contours:
        x, y, w, h = cv2.boundingRect(cnt)
        area = w * h
        aspect_ratio = w / h
        if area > max_area and 2 < aspect_ratio < 8:
            max_area = area
            display_contour = (x, y, w, h)
    
    return display_contour

def extract_weight_from_roi(frame, roi):
    x, y, w, h = roi
    display_img = frame[y:y+h, x:x+w]

    gray_display = cv2.cvtColor(display_img, cv2.COLOR_BGR2GRAY)
    _, display_thresh = cv2.threshold(gray_display, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)

    config = '--psm 7 -c tessedit_char_whitelist=0123456789.'
    text = pytesseract.image_to_string(display_thresh, config=config)
    text = ''.join(c for c in text if c.isdigit() or c == '.')
    
    try:
        weight = float(text)
    except:
        weight = None

    return weight, display_img

def main():
    cap = cv2.VideoCapture(0)
    if not cap.isOpened():
        print("Cannot open webcam")
        return

    # Use deque to store last N readings for stabilization
    history = deque(maxlen=5)
    captured_weight = None

    while True:
        ret, frame = cap.read()
        if not ret:
            break
        
        thresh = preprocess_display(frame)
        roi = find_display_roi(thresh)

        if roi is not None:
            weight, display_img = extract_weight_from_roi(frame, roi)
            if weight is not None:
                history.append(weight)
            
            # Draw display ROI
            x, y, w, h = roi
            cv2.rectangle(frame, (x, y), (x+w, y+h), (0,255,0), 2)
            
            # Check for stabilization: all recent weights within a small delta
            if len(history) == history.maxlen:
                if max(history) - min(history) < 0.05:  # stable if variation < 50 grams
                    if captured_weight != history[-1]:
                        captured_weight = history[-1]
                        print(f"Stabilized weight: {captured_weight} kg")
        
        # Display current reading on frame
        if history:
            cv2.putText(frame, f"Weight: {history[-1]:.2f} kg", (50,50),
                        cv2.FONT_HERSHEY_SIMPLEX, 1, (0,255,0), 2)
        
        cv2.imshow("Xiaomi S200 OCR", frame)
        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    cap.release()
    cv2.destroyAllWindows()

if __name__ == "__main__":
    main()