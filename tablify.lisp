;; tablify.lisp

;; metapixel

;; Copyright (C) 2003 Mark Probst

;; This program is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2, or (at your option)
;; any later version.

;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.

;; You should have received a copy of the GNU General Public License
;; along with this program; if not, you can either send email to this
;; program's maintainer or write to: The Free Software Foundation,
;; Inc.; 675 Massachusetts Avenue; Cambridge, MA 02139, USA.

(defun tablify-mosaic (mosaic)
  (let* ((width (second (second mosaic)))
	 (height (third (second mosaic)))
	 (metapixels (cdr (third mosaic)))
	 (names (make-array (list width height) :initial-element nil)))
    (dolist (pixel metapixels)
      (destructuring-bind (x y w h name)
	  pixel
	(setf (aref names x y) name)))
    (format t "<table cellpadding=0 cellspacing=0>~%")
    (dotimes (y height)
      (format t "<tr>~%")
      (dotimes (x width)
	(when (null (aref names x y))
	  (error "no metapixel for location (~A,~A)" x y))
	(format t "<td><img src=\"~A\"></td>~%" (aref names x y)))
      (format t "</tr>~%"))))

(defun tablify-mosaic-with-files (inname outname)
  (let ((mosaic (with-open-file (in inname :direction :input)
		  (read in))))
    (with-open-file (out outname :direction :output :if-exists :supersede)
      (let ((*standard-output* out))
	(tablify-mosaic mosaic)))))
