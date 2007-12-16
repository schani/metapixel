static float
COMPARE_FUNC_NAME (coeffs_union_t *coeffs, metapixel_t *pixel, float best_score,
		  float weight_factors[NUM_CHANNELS])
{
    int channel;
    float score = 0.0;

    for (channel = 0; channel < NUM_CHANNELS; ++channel)
    {
	int x, y;

	for (y = 0; y < NUM_SUBPIXEL_ROWS_COLS; ++y)
	{
	    for (x = 0; x < NUM_SUBPIXEL_ROWS_COLS; ++x)
	    {
		int coeffs_idx = y * NUM_SUBPIXEL_ROWS_COLS + x;
		int pixel_idx = FLIP_Y(y) * NUM_SUBPIXEL_ROWS_COLS + FLIP_X(x);

		float dist = (int)coeffs->subpixel.subpixels[channel * NUM_SUBPIXELS + coeffs_idx]
		    - (int)pixel->subpixels[channel * NUM_SUBPIXELS + pixel_idx];

		score += dist * dist * weight_factors[channel];

		if (score >= best_score)
		    return 1e99;
	    }
	}
    }

    return score;
}
