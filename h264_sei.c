/*
 * h264bitstream - a library for reading and writing H.264 video
 * Copyright (C) 2005-2007 Auroras Entertainment, LLC
 * Copyright (C) 2008-2011 Avail-TVN
 * Copyright (C) 2012 Alex Izvorski
 *
 * This file is written by Leslie Wang <wqyuwss@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "bs.h"
#include "h264_stream.h"
#include "h264_sei.h"

#include <stdio.h>
#include <stdlib.h> // malloc
#include <string.h> // memset

sei_t* sei_new()
{
    sei_t* s = (sei_t*)calloc(1, sizeof(sei_t));
    memset(s, 0, sizeof(sei_t));
    s->data = NULL;
    return s;
}

void sei_free(sei_t* s)
{
    switch( s->payloadType ) {
        case SEI_TYPE_SCALABILITY_INFO:
            if ( s->sei_svc != NULL ) free(s->sei_svc);
            break;
        case SEI_TYPE_PIC_TIMING:
            if ( s->sei_pic_timing != NULL ) free(s->sei_pic_timing);
            break;
        case SEI_TYPE_BUFFERING_PERIOD:
            if ( s->sei_buffering_period != NULL ) free(s->sei_buffering_period);
            break;
        case SEI_TYPE_RECOVERY_POINT:
            if ( s->sei_recovery_point != NULL ) free(s->sei_recovery_point);
            break;
        default:
            if ( s->data != NULL ) free(s->data);
    }
    free(s);
}

void read_sei_end_bits(h264_stream_t* h, bs_t* b )
{
    // if the message doesn't end at a byte border
    if ( !bs_byte_aligned( b ) )
    {
        if ( !bs_read_u1( b ) ) fprintf(stderr, "WARNING: bit_equal_to_one is 0!!!!\n");
        while ( ! bs_byte_aligned( b ) )
        {
            if ( bs_read_u1( b ) ) fprintf(stderr, "WARNING: bit_equal_to_zero is 1!!!!\n");
        }
    }
}

static const hrd_t* get_hrd_params( h264_stream_t* h )
{
    const hrd_t* hrd_params = NULL;

    if (h && h->sps && h->sps->vui_parameters_present_flag)
    {
        if (h->sps->vui.nal_hrd_parameters_present_flag)
        {
            hrd_params = (const hrd_t*)&h->sps->hrd_nal;
        }
        else if (h->sps->vui.vcl_hrd_parameters_present_flag)
        {
            hrd_params = (const hrd_t*)&h->sps->hrd_vcl;
        }
    }

    return hrd_params;
}


void read_sei_scalability_info( h264_stream_t* h, bs_t* b );
void read_sei_picture_timing( h264_stream_t* h, bs_t* b );
void read_sei_buffering_period( h264_stream_t* h, bs_t* b );
void read_sei_recovery_point( h264_stream_t* h, bs_t* b );
void read_sei_payload( h264_stream_t* h, bs_t* b );


// Appendix G.13.1.1 Scalability information SEI message syntax
void read_sei_scalability_info( h264_stream_t* h, bs_t* b )
{
    sei_scalability_info_t* sei_svc = h->sei->sei_svc;
    
    sei_svc->temporal_id_nesting_flag = bs_read_u1(b);
    sei_svc->priority_layer_info_present_flag = bs_read_u1(b);
    sei_svc->priority_id_setting_flag = bs_read_u1(b);
    sei_svc->num_layers_minus1 = bs_read_ue(b);
    
    for( int i = 0; i <= sei_svc->num_layers_minus1; i++ ) {
        sei_svc->layers[i].layer_id = bs_read_ue(b);
        sei_svc->layers[i].priority_id = bs_read_u(b, 6);
        sei_svc->layers[i].discardable_flag = bs_read_u1(b);
        sei_svc->layers[i].dependency_id = bs_read_u(b, 3);
        sei_svc->layers[i].quality_id = bs_read_u(b, 4);
        sei_svc->layers[i].temporal_id = bs_read_u(b, 3);
        sei_svc->layers[i].sub_pic_layer_flag = bs_read_u1(b);
        sei_svc->layers[i].sub_region_layer_flag = bs_read_u1(b);
        sei_svc->layers[i].iroi_division_info_present_flag = bs_read_u1(b);
        sei_svc->layers[i].profile_level_info_present_flag = bs_read_u1(b);
        sei_svc->layers[i].bitrate_info_present_flag = bs_read_u1(b);
        sei_svc->layers[i].frm_rate_info_present_flag = bs_read_u1(b);
        sei_svc->layers[i].frm_size_info_present_flag = bs_read_u1(b);
        sei_svc->layers[i].layer_dependency_info_present_flag = bs_read_u1(b);
        sei_svc->layers[i].parameter_sets_info_present_flag = bs_read_u1(b);
        sei_svc->layers[i].bitstream_restriction_info_present_flag = bs_read_u1(b);
        sei_svc->layers[i].exact_inter_layer_pred_flag = bs_read_u1(b);
        if( sei_svc->layers[i].sub_pic_layer_flag ||
            sei_svc->layers[i].iroi_division_info_present_flag )
        {
            sei_svc->layers[i].exact_sample_value_match_flag = bs_read_u1(b);
        }
        sei_svc->layers[i].layer_conversion_flag = bs_read_u1(b);
        sei_svc->layers[i].layer_output_flag = bs_read_u1(b);
        if( sei_svc->layers[i].profile_level_info_present_flag )
        {
            sei_svc->layers[i].layer_profile_level_idc = bs_read_u(b, 24);
        }
        if( sei_svc->layers[i].bitrate_info_present_flag )
        {
            sei_svc->layers[i].avg_bitrate = bs_read_u(b, 16);
            sei_svc->layers[i].max_bitrate_layer = bs_read_u(b, 16);
            sei_svc->layers[i].max_bitrate_layer_representation = bs_read_u(b, 16);
            sei_svc->layers[i].max_bitrate_calc_window = bs_read_u(b, 16);
        }
        if( sei_svc->layers[i].frm_rate_info_present_flag )
        {
            sei_svc->layers[i].constant_frm_rate_idc = bs_read_u(b, 2);
            sei_svc->layers[i].avg_frm_rate = bs_read_u(b, 16);
        }
        if( sei_svc->layers[i].frm_size_info_present_flag ||
            sei_svc->layers[i].iroi_division_info_present_flag )
        {
            sei_svc->layers[i].frm_width_in_mbs_minus1 = bs_read_ue(b);
            sei_svc->layers[i].frm_height_in_mbs_minus1 = bs_read_ue(b);
        }
        if( sei_svc->layers[i].sub_region_layer_flag )
        {
            sei_svc->layers[i].base_region_layer_id = bs_read_ue(b);
            sei_svc->layers[i].dynamic_rect_flag = bs_read_u1(b);
            if( sei_svc->layers[i].dynamic_rect_flag )
            {
                sei_svc->layers[i].horizontal_offset = bs_read_u(b, 16);
                sei_svc->layers[i].vertical_offset = bs_read_u(b, 16);
                sei_svc->layers[i].region_width = bs_read_u(b, 16);
                sei_svc->layers[i].region_height = bs_read_u(b, 16);
            }
        }
        if( sei_svc->layers[i].sub_pic_layer_flag )
        {
            sei_svc->layers[i].roi_id = bs_read_ue(b);
        }
        if( sei_svc->layers[i].iroi_division_info_present_flag )
        {
            sei_svc->layers[i].iroi_grid_flag = bs_read_u1(b);
            if( sei_svc->layers[i].iroi_grid_flag )
            {
                sei_svc->layers[i].grid_width_in_mbs_minus1 = bs_read_ue(b);
                sei_svc->layers[i].grid_height_in_mbs_minus1 = bs_read_ue(b);
            }
            else
            {
                sei_svc->layers[i].num_rois_minus1 = bs_read_ue(b);
                
                for( int j = 0; j <= sei_svc->layers[i].num_rois_minus1; j++ )
                {
                    sei_svc->layers[i].roi[j].first_mb_in_roi = bs_read_ue(b);
                    sei_svc->layers[i].roi[j].roi_width_in_mbs_minus1 = bs_read_ue(b);
                    sei_svc->layers[i].roi[j].roi_height_in_mbs_minus1 = bs_read_ue(b);
                }
            }
        }
        if( sei_svc->layers[i].layer_dependency_info_present_flag )
        {
            sei_svc->layers[i].num_directly_dependent_layers = bs_read_ue(b);
            for( int j = 0; j < sei_svc->layers[i].num_directly_dependent_layers; j++ )
            {
                sei_svc->layers[i].directly_dependent_layer_id_delta_minus1[j] = bs_read_ue(b);
            }
        }
        else
        {
            sei_svc->layers[i].layer_dependency_info_src_layer_id_delta = bs_read_ue(b);
        }
        if( sei_svc->layers[i].parameter_sets_info_present_flag )
        {
            sei_svc->layers[i].num_seq_parameter_sets = bs_read_ue(b);
            for( int j = 0; j < sei_svc->layers[i].num_seq_parameter_sets; j++ )
            {
                sei_svc->layers[i].seq_parameter_set_id_delta[j] = bs_read_ue(b);
            }
            sei_svc->layers[i].num_subset_seq_parameter_sets = bs_read_ue(b);
            for( int j = 0; j < sei_svc->layers[i].num_subset_seq_parameter_sets; j++ )
            {
                sei_svc->layers[i].subset_seq_parameter_set_id_delta[j] = bs_read_ue(b);
            }
            sei_svc->layers[i].num_pic_parameter_sets_minus1 = bs_read_ue(b);
            for( int j = 0; j < sei_svc->layers[i].num_pic_parameter_sets_minus1; j++ )
            {
                sei_svc->layers[i].pic_parameter_set_id_delta[j] = bs_read_ue(b);
            }
        }
        else
        {
            sei_svc->layers[i].parameter_sets_info_src_layer_id_delta = bs_read_ue(b);
        }
        if( sei_svc->layers[i].bitstream_restriction_info_present_flag )
        {
            sei_svc->layers[i].motion_vectors_over_pic_boundaries_flag = bs_read_u1(b);
            sei_svc->layers[i].max_bytes_per_pic_denom = bs_read_ue(b);
            sei_svc->layers[i].max_bits_per_mb_denom = bs_read_ue(b);
            sei_svc->layers[i].log2_max_mv_length_horizontal = bs_read_ue(b);
            sei_svc->layers[i].log2_max_mv_length_vertical = bs_read_ue(b);
            sei_svc->layers[i].max_num_reorder_frames = bs_read_ue(b);
            sei_svc->layers[i].max_dec_frame_buffering = bs_read_ue(b);
        }
        if( sei_svc->layers[i].layer_conversion_flag )
        {
            sei_svc->layers[i].conversion_type_idc = bs_read_ue(b);
            for( int j = 0; j < 2; j++ )
            {
                sei_svc->layers[i].rewriting_info_flag[j] = bs_read_u(b, 1);
                if( sei_svc->layers[i].rewriting_info_flag[j] )
                {
                    sei_svc->layers[i].rewriting_profile_level_idc[j] = bs_read_u(b, 24);
                    sei_svc->layers[i].rewriting_avg_bitrate[j] = bs_read_u(b, 16);
                    sei_svc->layers[i].rewriting_max_bitrate[j] = bs_read_u(b, 16);
                }
            }
        }
    }

    if( sei_svc->priority_layer_info_present_flag )
    {
        sei_svc->pr_num_dIds_minus1 = bs_read_ue(b);
        
        for( int i = 0; i <= sei_svc->pr_num_dIds_minus1; i++ ) {
            sei_svc->pr[i].pr_dependency_id = bs_read_u(b, 3);
            sei_svc->pr[i].pr_num_minus1 = bs_read_ue(b);
            for( int j = 0; j <= sei_svc->pr[i].pr_num_minus1; j++ )
            {
                sei_svc->pr[i].pr_info[j].pr_id = bs_read_ue(b);
                sei_svc->pr[i].pr_info[j].pr_profile_level_idc = bs_read_u(b, 24);
                sei_svc->pr[i].pr_info[j].pr_avg_bitrate = bs_read_u(b, 16);
                sei_svc->pr[i].pr_info[j].pr_max_bitrate = bs_read_u(b, 16);
            }
        }
        
    }

}

// Appendix D.1.3 Picture timing SEI message syntax
void read_sei_picture_timing( h264_stream_t* h, bs_t* b )
{
    sei_picture_timing_t* sei_pic_timing = h->sei->sei_pic_timing;

    const hrd_t* hrd_params = get_hrd_params( h );

    if ( hrd_params )
    {
        sei_pic_timing->cpb_removal_delay = bs_read_u(b, hrd_params->cpb_removal_delay_length_minus1 + 1);
        sei_pic_timing->dpb_output_delay = bs_read_u(b, hrd_params->dpb_output_delay_length_minus1 + 1);
    }

    if ( h && h->sps && h->sps->vui_parameters_present_flag && h->sps->vui.pic_struct_present_flag )
    {
        sei_pic_timing->pic_struct = bs_read_u(b, 4);

        const int num_clock_ts = pic_struct_to_num_clock_ts[sei_pic_timing->pic_struct];

        for ( int i = 0; i < num_clock_ts; i++ )
        {
            sei_pic_timing->clock_timestamps[i].clock_timestamp_flag = bs_read_u1(b);

            if ( sei_pic_timing->clock_timestamps[i].clock_timestamp_flag )
            {
                sei_pic_timing->clock_timestamps[i].ct_type = bs_read_u(b, 2);
                sei_pic_timing->clock_timestamps[i].nuit_field_based_flag = bs_read_u1(b);
                sei_pic_timing->clock_timestamps[i].counting_type = bs_read_u(b, 5);
                sei_pic_timing->clock_timestamps[i].full_timestamp_flag = bs_read_u1(b);
                sei_pic_timing->clock_timestamps[i].discontinuity_flag = bs_read_u1(b);
                sei_pic_timing->clock_timestamps[i].cnt_dropped_flag = bs_read_u1(b);
                sei_pic_timing->clock_timestamps[i].n_frames = bs_read_u8(b);

                if ( sei_pic_timing->clock_timestamps[i].full_timestamp_flag )
                {
                    sei_pic_timing->clock_timestamps[i].seconds_value = bs_read_u(b, 6);
                    sei_pic_timing->clock_timestamps[i].minutes_value = bs_read_u(b, 6);
                    sei_pic_timing->clock_timestamps[i].hours_value = bs_read_u(b, 5);
                }
                else
                {
                    sei_pic_timing->clock_timestamps[i].seconds_flag = bs_read_u1(b);

                    if ( sei_pic_timing->clock_timestamps[i].seconds_flag )
                    {
                        sei_pic_timing->clock_timestamps[i].seconds_value = bs_read_u(b, 6);
                        sei_pic_timing->clock_timestamps[i].minutes_flag = bs_read_u1(b);

                        if ( sei_pic_timing->clock_timestamps[i].minutes_flag )
                        {
                            sei_pic_timing->clock_timestamps[i].minutes_value = bs_read_u(b, 6);
                            sei_pic_timing->clock_timestamps[i].hours_flag = bs_read_u1(b);

                            if ( sei_pic_timing->clock_timestamps[i].hours_flag )
                            {
                                sei_pic_timing->clock_timestamps[i].hours_value = bs_read_u(b, 5);
                            }
                        }
                    }
                }
            }

            const int time_offset_length = hrd_params ? hrd_params->time_offset_length : 24;

            if ( time_offset_length )
            {
                sei_pic_timing->clock_timestamps[i].time_offset = bs_read_i(b, time_offset_length);
            }
        }
    }
}

// Appendix D.1.2 Buffering period SEI message syntax
void read_sei_buffering_period( h264_stream_t* h, bs_t* b )
{
    sei_buffering_period_t* sei_buffering_period = h->sei->sei_buffering_period;

    sei_buffering_period->sps_id = bs_read_ue(b);

    if ( h && h->sps )
    {
        if ( h->sps->vui_parameters_present_flag && h->sps->vui.nal_hrd_parameters_present_flag )
        {
            const int length = h->sps->hrd_nal.initial_cpb_removal_delay_length_minus1 + 1;

            for ( int SchedSelIdx = 0; SchedSelIdx <= h->sps->hrd_nal.cpb_cnt_minus1; SchedSelIdx++ )
            {
                sei_buffering_period->initial_cpb_removal_delay[ SchedSelIdx ] = bs_read_u(b, length);
                sei_buffering_period->initial_cpb_delay_offset[ SchedSelIdx ] = bs_read_u(b, length);
            }
        }

        if ( h->sps->vui_parameters_present_flag && h->sps->vui.vcl_hrd_parameters_present_flag )
        {
            const int length = h->sps->hrd_vcl.initial_cpb_removal_delay_length_minus1 + 1;

            for ( int SchedSelIdx = 0; SchedSelIdx <= h->sps->hrd_vcl.cpb_cnt_minus1; SchedSelIdx++ )
            {
                sei_buffering_period->initial_cpb_removal_delay[ SchedSelIdx ] = bs_read_u(b, length);
                sei_buffering_period->initial_cpb_delay_offset[ SchedSelIdx ] = bs_read_u(b, length);
            }
        }
    }
}

// Appendix D.1.8 Recovery point SEI message syntax
void read_sei_recovery_point( h264_stream_t* h, bs_t* b )
{
    sei_recovery_point_t* sei_recovery_point = h->sei->sei_recovery_point;

    sei_recovery_point->recovery_frame_cnt = bs_read_ue(b);
    sei_recovery_point->exact_match_flag = bs_read_u1(b);
    sei_recovery_point->broken_link_flag = bs_read_u1(b);
    sei_recovery_point->changing_slice_grp_idc = bs_read_u(b, 2);
}

// D.1 SEI payload syntax
void read_sei_payload( h264_stream_t* h, bs_t* b )
{
    sei_t* s = h->sei;
    
    int i;
    switch( s->payloadType )
    {
        case SEI_TYPE_SCALABILITY_INFO:
            if( 1 )
            {
                s->sei_svc = (sei_scalability_info_t*)calloc( 1, sizeof(sei_scalability_info_t) );
            }
            read_sei_scalability_info( h, b );
            break;
        case SEI_TYPE_PIC_TIMING:
            if ( 1 )
            {
                s->sei_pic_timing = (sei_picture_timing_t*)calloc( 1, sizeof(sei_picture_timing_t) );
            }
            read_sei_picture_timing( h, b );
            break;
        case SEI_TYPE_BUFFERING_PERIOD:
            if ( 1 )
            {
                s->sei_buffering_period = (sei_buffering_period_t*)calloc( 1, sizeof(sei_buffering_period_t) );
            }
            read_sei_buffering_period( h, b );
            break;
        case SEI_TYPE_RECOVERY_POINT:
            if ( 1 )
            {
                s->sei_recovery_point = (sei_recovery_point_t*)calloc( 1, sizeof(sei_recovery_point_t) );
            }
            read_sei_recovery_point( h, b );
            break;
        default:
            if( 1 )
            {
                s->data = (uint8_t*)calloc(1, s->payloadSize);
            }
            
            for ( i = 0; i < s->payloadSize; i++ )
            {
                s->data[i] = bs_read_u8(b);
            }
    }

    if( 1 )
        read_sei_end_bits(h, b);
}


void write_sei_scalability_info( h264_stream_t* h, bs_t* b );
void write_sei_picture_timing( h264_stream_t* h, bs_t* b );
void write_sei_buffering_period( h264_stream_t* h, bs_t* b );
void write_sei_recovery_point( h264_stream_t* h, bs_t* b );
void write_sei_payload( h264_stream_t* h, bs_t* b );


// Appendix G.13.1.1 Scalability information SEI message syntax
void write_sei_scalability_info( h264_stream_t* h, bs_t* b )
{
    sei_scalability_info_t* sei_svc = h->sei->sei_svc;
    
    bs_write_u1(b, sei_svc->temporal_id_nesting_flag);
    bs_write_u1(b, sei_svc->priority_layer_info_present_flag);
    bs_write_u1(b, sei_svc->priority_id_setting_flag);
    bs_write_ue(b, sei_svc->num_layers_minus1);
    
    for( int i = 0; i <= sei_svc->num_layers_minus1; i++ ) {
        bs_write_ue(b, sei_svc->layers[i].layer_id);
        bs_write_u(b, 6, sei_svc->layers[i].priority_id);
        bs_write_u1(b, sei_svc->layers[i].discardable_flag);
        bs_write_u(b, 3, sei_svc->layers[i].dependency_id);
        bs_write_u(b, 4, sei_svc->layers[i].quality_id);
        bs_write_u(b, 3, sei_svc->layers[i].temporal_id);
        bs_write_u1(b, sei_svc->layers[i].sub_pic_layer_flag);
        bs_write_u1(b, sei_svc->layers[i].sub_region_layer_flag);
        bs_write_u1(b, sei_svc->layers[i].iroi_division_info_present_flag);
        bs_write_u1(b, sei_svc->layers[i].profile_level_info_present_flag);
        bs_write_u1(b, sei_svc->layers[i].bitrate_info_present_flag);
        bs_write_u1(b, sei_svc->layers[i].frm_rate_info_present_flag);
        bs_write_u1(b, sei_svc->layers[i].frm_size_info_present_flag);
        bs_write_u1(b, sei_svc->layers[i].layer_dependency_info_present_flag);
        bs_write_u1(b, sei_svc->layers[i].parameter_sets_info_present_flag);
        bs_write_u1(b, sei_svc->layers[i].bitstream_restriction_info_present_flag);
        bs_write_u1(b, sei_svc->layers[i].exact_inter_layer_pred_flag);
        if( sei_svc->layers[i].sub_pic_layer_flag ||
            sei_svc->layers[i].iroi_division_info_present_flag )
        {
            bs_write_u1(b, sei_svc->layers[i].exact_sample_value_match_flag);
        }
        bs_write_u1(b, sei_svc->layers[i].layer_conversion_flag);
        bs_write_u1(b, sei_svc->layers[i].layer_output_flag);
        if( sei_svc->layers[i].profile_level_info_present_flag )
        {
            bs_write_u(b, 24, sei_svc->layers[i].layer_profile_level_idc);
        }
        if( sei_svc->layers[i].bitrate_info_present_flag )
        {
            bs_write_u(b, 16, sei_svc->layers[i].avg_bitrate);
            bs_write_u(b, 16, sei_svc->layers[i].max_bitrate_layer);
            bs_write_u(b, 16, sei_svc->layers[i].max_bitrate_layer_representation);
            bs_write_u(b, 16, sei_svc->layers[i].max_bitrate_calc_window);
        }
        if( sei_svc->layers[i].frm_rate_info_present_flag )
        {
            bs_write_u(b, 2, sei_svc->layers[i].constant_frm_rate_idc);
            bs_write_u(b, 16, sei_svc->layers[i].avg_frm_rate);
        }
        if( sei_svc->layers[i].frm_size_info_present_flag ||
            sei_svc->layers[i].iroi_division_info_present_flag )
        {
            bs_write_ue(b, sei_svc->layers[i].frm_width_in_mbs_minus1);
            bs_write_ue(b, sei_svc->layers[i].frm_height_in_mbs_minus1);
        }
        if( sei_svc->layers[i].sub_region_layer_flag )
        {
            bs_write_ue(b, sei_svc->layers[i].base_region_layer_id);
            bs_write_u1(b, sei_svc->layers[i].dynamic_rect_flag);
            if( sei_svc->layers[i].dynamic_rect_flag )
            {
                bs_write_u(b, 16, sei_svc->layers[i].horizontal_offset);
                bs_write_u(b, 16, sei_svc->layers[i].vertical_offset);
                bs_write_u(b, 16, sei_svc->layers[i].region_width);
                bs_write_u(b, 16, sei_svc->layers[i].region_height);
            }
        }
        if( sei_svc->layers[i].sub_pic_layer_flag )
        {
            bs_write_ue(b, sei_svc->layers[i].roi_id);
        }
        if( sei_svc->layers[i].iroi_division_info_present_flag )
        {
            bs_write_u1(b, sei_svc->layers[i].iroi_grid_flag);
            if( sei_svc->layers[i].iroi_grid_flag )
            {
                bs_write_ue(b, sei_svc->layers[i].grid_width_in_mbs_minus1);
                bs_write_ue(b, sei_svc->layers[i].grid_height_in_mbs_minus1);
            }
            else
            {
                bs_write_ue(b, sei_svc->layers[i].num_rois_minus1);
                
                for( int j = 0; j <= sei_svc->layers[i].num_rois_minus1; j++ )
                {
                    bs_write_ue(b, sei_svc->layers[i].roi[j].first_mb_in_roi);
                    bs_write_ue(b, sei_svc->layers[i].roi[j].roi_width_in_mbs_minus1);
                    bs_write_ue(b, sei_svc->layers[i].roi[j].roi_height_in_mbs_minus1);
                }
            }
        }
        if( sei_svc->layers[i].layer_dependency_info_present_flag )
        {
            bs_write_ue(b, sei_svc->layers[i].num_directly_dependent_layers);
            for( int j = 0; j < sei_svc->layers[i].num_directly_dependent_layers; j++ )
            {
                bs_write_ue(b, sei_svc->layers[i].directly_dependent_layer_id_delta_minus1[j]);
            }
        }
        else
        {
            bs_write_ue(b, sei_svc->layers[i].layer_dependency_info_src_layer_id_delta);
        }
        if( sei_svc->layers[i].parameter_sets_info_present_flag )
        {
            bs_write_ue(b, sei_svc->layers[i].num_seq_parameter_sets);
            for( int j = 0; j < sei_svc->layers[i].num_seq_parameter_sets; j++ )
            {
                bs_write_ue(b, sei_svc->layers[i].seq_parameter_set_id_delta[j]);
            }
            bs_write_ue(b, sei_svc->layers[i].num_subset_seq_parameter_sets);
            for( int j = 0; j < sei_svc->layers[i].num_subset_seq_parameter_sets; j++ )
            {
                bs_write_ue(b, sei_svc->layers[i].subset_seq_parameter_set_id_delta[j]);
            }
            bs_write_ue(b, sei_svc->layers[i].num_pic_parameter_sets_minus1);
            for( int j = 0; j < sei_svc->layers[i].num_pic_parameter_sets_minus1; j++ )
            {
                bs_write_ue(b, sei_svc->layers[i].pic_parameter_set_id_delta[j]);
            }
        }
        else
        {
            bs_write_ue(b, sei_svc->layers[i].parameter_sets_info_src_layer_id_delta);
        }
        if( sei_svc->layers[i].bitstream_restriction_info_present_flag )
        {
            bs_write_u1(b, sei_svc->layers[i].motion_vectors_over_pic_boundaries_flag);
            bs_write_ue(b, sei_svc->layers[i].max_bytes_per_pic_denom);
            bs_write_ue(b, sei_svc->layers[i].max_bits_per_mb_denom);
            bs_write_ue(b, sei_svc->layers[i].log2_max_mv_length_horizontal);
            bs_write_ue(b, sei_svc->layers[i].log2_max_mv_length_vertical);
            bs_write_ue(b, sei_svc->layers[i].max_num_reorder_frames);
            bs_write_ue(b, sei_svc->layers[i].max_dec_frame_buffering);
        }
        if( sei_svc->layers[i].layer_conversion_flag )
        {
            bs_write_ue(b, sei_svc->layers[i].conversion_type_idc);
            for( int j = 0; j < 2; j++ )
            {
                bs_write_u(b, 1, sei_svc->layers[i].rewriting_info_flag[j]);
                if( sei_svc->layers[i].rewriting_info_flag[j] )
                {
                    bs_write_u(b, 24, sei_svc->layers[i].rewriting_profile_level_idc[j]);
                    bs_write_u(b, 16, sei_svc->layers[i].rewriting_avg_bitrate[j]);
                    bs_write_u(b, 16, sei_svc->layers[i].rewriting_max_bitrate[j]);
                }
            }
        }
    }

    if( sei_svc->priority_layer_info_present_flag )
    {
        bs_write_ue(b, sei_svc->pr_num_dIds_minus1);
        
        for( int i = 0; i <= sei_svc->pr_num_dIds_minus1; i++ ) {
            bs_write_u(b, 3, sei_svc->pr[i].pr_dependency_id);
            bs_write_ue(b, sei_svc->pr[i].pr_num_minus1);
            for( int j = 0; j <= sei_svc->pr[i].pr_num_minus1; j++ )
            {
                bs_write_ue(b, sei_svc->pr[i].pr_info[j].pr_id);
                bs_write_u(b, 24, sei_svc->pr[i].pr_info[j].pr_profile_level_idc);
                bs_write_u(b, 16, sei_svc->pr[i].pr_info[j].pr_avg_bitrate);
                bs_write_u(b, 16, sei_svc->pr[i].pr_info[j].pr_max_bitrate);
            }
        }
        
    }

}

// Appendix D.1.3 Picture timing SEI message syntax
void write_sei_picture_timing( h264_stream_t* h, bs_t* b )
{
    sei_picture_timing_t* sei_pic_timing = h->sei->sei_pic_timing;

    const hrd_t* hrd_params = get_hrd_params( h );

    if ( hrd_params )
    {
        bs_write_u(b, hrd_params->cpb_removal_delay_length_minus1 + 1, sei_pic_timing->cpb_removal_delay);
        bs_write_u(b, hrd_params->dpb_output_delay_length_minus1 + 1, sei_pic_timing->dpb_output_delay);
    }

    if ( h && h->sps && h->sps->vui_parameters_present_flag && h->sps->vui.pic_struct_present_flag )
    {
        bs_write_u(b, 4, sei_pic_timing->pic_struct);

        const int num_clock_ts = pic_struct_to_num_clock_ts[sei_pic_timing->pic_struct];

        for ( int i = 0; i < num_clock_ts; i++ )
        {
            bs_write_u1(b, sei_pic_timing->clock_timestamps[i].clock_timestamp_flag);

            if ( sei_pic_timing->clock_timestamps[i].clock_timestamp_flag )
            {
                bs_write_u(b, 2, sei_pic_timing->clock_timestamps[i].ct_type);
                bs_write_u1(b, sei_pic_timing->clock_timestamps[i].nuit_field_based_flag);
                bs_write_u(b, 5, sei_pic_timing->clock_timestamps[i].counting_type);
                bs_write_u1(b, sei_pic_timing->clock_timestamps[i].full_timestamp_flag);
                bs_write_u1(b, sei_pic_timing->clock_timestamps[i].discontinuity_flag);
                bs_write_u1(b, sei_pic_timing->clock_timestamps[i].cnt_dropped_flag);
                bs_write_u8(b, sei_pic_timing->clock_timestamps[i].n_frames);

                if ( sei_pic_timing->clock_timestamps[i].full_timestamp_flag )
                {
                    bs_write_u(b, 6, sei_pic_timing->clock_timestamps[i].seconds_value);
                    bs_write_u(b, 6, sei_pic_timing->clock_timestamps[i].minutes_value);
                    bs_write_u(b, 5, sei_pic_timing->clock_timestamps[i].hours_value);
                }
                else
                {
                    bs_write_u1(b, sei_pic_timing->clock_timestamps[i].seconds_flag);

                    if ( sei_pic_timing->clock_timestamps[i].seconds_flag )
                    {
                        bs_write_u(b, 6, sei_pic_timing->clock_timestamps[i].seconds_value);
                        bs_write_u1(b, sei_pic_timing->clock_timestamps[i].minutes_flag);

                        if ( sei_pic_timing->clock_timestamps[i].minutes_flag )
                        {
                            bs_write_u(b, 6, sei_pic_timing->clock_timestamps[i].minutes_value);
                            bs_write_u1(b, sei_pic_timing->clock_timestamps[i].hours_flag);

                            if ( sei_pic_timing->clock_timestamps[i].hours_flag )
                            {
                                bs_write_u(b, 5, sei_pic_timing->clock_timestamps[i].hours_value);
                            }
                        }
                    }
                }
            }

            const int time_offset_length = hrd_params ? hrd_params->time_offset_length : 24;

            if ( time_offset_length )
            {
                bs_write_i(b, time_offset_length, sei_pic_timing->clock_timestamps[i].time_offset);
            }
        }
    }
}

// Appendix D.1.2 Buffering period SEI message syntax
void write_sei_buffering_period( h264_stream_t* h, bs_t* b )
{
    sei_buffering_period_t* sei_buffering_period = h->sei->sei_buffering_period;

    bs_write_ue(b, sei_buffering_period->sps_id);

    if ( h && h->sps )
    {
        if ( h->sps->vui_parameters_present_flag && h->sps->vui.nal_hrd_parameters_present_flag )
        {
            const int length = h->sps->hrd_nal.initial_cpb_removal_delay_length_minus1 + 1;

            for ( int SchedSelIdx = 0; SchedSelIdx <= h->sps->hrd_nal.cpb_cnt_minus1; SchedSelIdx++ )
            {
                bs_write_u(b, length, sei_buffering_period->initial_cpb_removal_delay[ SchedSelIdx ]);
                bs_write_u(b, length, sei_buffering_period->initial_cpb_delay_offset[ SchedSelIdx ]);
            }
        }

        if ( h->sps->vui_parameters_present_flag && h->sps->vui.vcl_hrd_parameters_present_flag )
        {
            const int length = h->sps->hrd_vcl.initial_cpb_removal_delay_length_minus1 + 1;

            for ( int SchedSelIdx = 0; SchedSelIdx <= h->sps->hrd_vcl.cpb_cnt_minus1; SchedSelIdx++ )
            {
                bs_write_u(b, length, sei_buffering_period->initial_cpb_removal_delay[ SchedSelIdx ]);
                bs_write_u(b, length, sei_buffering_period->initial_cpb_delay_offset[ SchedSelIdx ]);
            }
        }
    }
}

// Appendix D.1.8 Recovery point SEI message syntax
void write_sei_recovery_point( h264_stream_t* h, bs_t* b )
{
    sei_recovery_point_t* sei_recovery_point = h->sei->sei_recovery_point;

    bs_write_ue(b, sei_recovery_point->recovery_frame_cnt);
    bs_write_u1(b, sei_recovery_point->exact_match_flag);
    bs_write_u1(b, sei_recovery_point->broken_link_flag);
    bs_write_u(b, 2, sei_recovery_point->changing_slice_grp_idc);
}

// D.1 SEI payload syntax
void write_sei_payload( h264_stream_t* h, bs_t* b )
{
    sei_t* s = h->sei;
    
    int i;
    switch( s->payloadType )
    {
        case SEI_TYPE_SCALABILITY_INFO:
            if( 0 )
            {
                s->sei_svc = (sei_scalability_info_t*)calloc( 1, sizeof(sei_scalability_info_t) );
            }
            write_sei_scalability_info( h, b );
            break;
        case SEI_TYPE_PIC_TIMING:
            if ( 0 )
            {
                s->sei_pic_timing = (sei_picture_timing_t*)calloc( 1, sizeof(sei_picture_timing_t) );
            }
            write_sei_picture_timing( h, b );
            break;
        case SEI_TYPE_BUFFERING_PERIOD:
            if ( 0 )
            {
                s->sei_buffering_period = (sei_buffering_period_t*)calloc( 1, sizeof(sei_buffering_period_t) );
            }
            write_sei_buffering_period( h, b );
            break;
        case SEI_TYPE_RECOVERY_POINT:
            if ( 0 )
            {
                s->sei_recovery_point = (sei_recovery_point_t*)calloc( 1, sizeof(sei_recovery_point_t) );
            }
            write_sei_recovery_point( h, b );
            break;
        default:
            if( 0 )
            {
                s->data = (uint8_t*)calloc(1, s->payloadSize);
            }
            
            for ( i = 0; i < s->payloadSize; i++ )
            {
                bs_write_u8(b, s->data[i]);
            }
    }

    if( 0 )
        read_sei_end_bits(h, b);
}


void read_debug_sei_scalability_info( h264_stream_t* h, bs_t* b );
void read_debug_sei_picture_timing( h264_stream_t* h, bs_t* b );
void read_debug_sei_buffering_period( h264_stream_t* h, bs_t* b );
void read_debug_sei_recovery_point( h264_stream_t* h, bs_t* b );
void read_debug_sei_payload( h264_stream_t* h, bs_t* b );


// Appendix G.13.1.1 Scalability information SEI message syntax
void read_debug_sei_scalability_info( h264_stream_t* h, bs_t* b )
{
    sei_scalability_info_t* sei_svc = h->sei->sei_svc;
    
    printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->temporal_id_nesting_flag = bs_read_u1(b); printf("sei_svc->temporal_id_nesting_flag: %d \n", sei_svc->temporal_id_nesting_flag);
    printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->priority_layer_info_present_flag = bs_read_u1(b); printf("sei_svc->priority_layer_info_present_flag: %d \n", sei_svc->priority_layer_info_present_flag);
    printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->priority_id_setting_flag = bs_read_u1(b); printf("sei_svc->priority_id_setting_flag: %d \n", sei_svc->priority_id_setting_flag);
    printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->num_layers_minus1 = bs_read_ue(b); printf("sei_svc->num_layers_minus1: %d \n", sei_svc->num_layers_minus1);
    
    for( int i = 0; i <= sei_svc->num_layers_minus1; i++ ) {
        printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].layer_id = bs_read_ue(b); printf("sei_svc->layers[i].layer_id: %d \n", sei_svc->layers[i].layer_id);
        printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].priority_id = bs_read_u(b, 6); printf("sei_svc->layers[i].priority_id: %d \n", sei_svc->layers[i].priority_id);
        printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].discardable_flag = bs_read_u1(b); printf("sei_svc->layers[i].discardable_flag: %d \n", sei_svc->layers[i].discardable_flag);
        printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].dependency_id = bs_read_u(b, 3); printf("sei_svc->layers[i].dependency_id: %d \n", sei_svc->layers[i].dependency_id);
        printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].quality_id = bs_read_u(b, 4); printf("sei_svc->layers[i].quality_id: %d \n", sei_svc->layers[i].quality_id);
        printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].temporal_id = bs_read_u(b, 3); printf("sei_svc->layers[i].temporal_id: %d \n", sei_svc->layers[i].temporal_id);
        printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].sub_pic_layer_flag = bs_read_u1(b); printf("sei_svc->layers[i].sub_pic_layer_flag: %d \n", sei_svc->layers[i].sub_pic_layer_flag);
        printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].sub_region_layer_flag = bs_read_u1(b); printf("sei_svc->layers[i].sub_region_layer_flag: %d \n", sei_svc->layers[i].sub_region_layer_flag);
        printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].iroi_division_info_present_flag = bs_read_u1(b); printf("sei_svc->layers[i].iroi_division_info_present_flag: %d \n", sei_svc->layers[i].iroi_division_info_present_flag);
        printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].profile_level_info_present_flag = bs_read_u1(b); printf("sei_svc->layers[i].profile_level_info_present_flag: %d \n", sei_svc->layers[i].profile_level_info_present_flag);
        printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].bitrate_info_present_flag = bs_read_u1(b); printf("sei_svc->layers[i].bitrate_info_present_flag: %d \n", sei_svc->layers[i].bitrate_info_present_flag);
        printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].frm_rate_info_present_flag = bs_read_u1(b); printf("sei_svc->layers[i].frm_rate_info_present_flag: %d \n", sei_svc->layers[i].frm_rate_info_present_flag);
        printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].frm_size_info_present_flag = bs_read_u1(b); printf("sei_svc->layers[i].frm_size_info_present_flag: %d \n", sei_svc->layers[i].frm_size_info_present_flag);
        printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].layer_dependency_info_present_flag = bs_read_u1(b); printf("sei_svc->layers[i].layer_dependency_info_present_flag: %d \n", sei_svc->layers[i].layer_dependency_info_present_flag);
        printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].parameter_sets_info_present_flag = bs_read_u1(b); printf("sei_svc->layers[i].parameter_sets_info_present_flag: %d \n", sei_svc->layers[i].parameter_sets_info_present_flag);
        printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].bitstream_restriction_info_present_flag = bs_read_u1(b); printf("sei_svc->layers[i].bitstream_restriction_info_present_flag: %d \n", sei_svc->layers[i].bitstream_restriction_info_present_flag);
        printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].exact_inter_layer_pred_flag = bs_read_u1(b); printf("sei_svc->layers[i].exact_inter_layer_pred_flag: %d \n", sei_svc->layers[i].exact_inter_layer_pred_flag);
        if( sei_svc->layers[i].sub_pic_layer_flag ||
            sei_svc->layers[i].iroi_division_info_present_flag )
        {
            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].exact_sample_value_match_flag = bs_read_u1(b); printf("sei_svc->layers[i].exact_sample_value_match_flag: %d \n", sei_svc->layers[i].exact_sample_value_match_flag);
        }
        printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].layer_conversion_flag = bs_read_u1(b); printf("sei_svc->layers[i].layer_conversion_flag: %d \n", sei_svc->layers[i].layer_conversion_flag);
        printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].layer_output_flag = bs_read_u1(b); printf("sei_svc->layers[i].layer_output_flag: %d \n", sei_svc->layers[i].layer_output_flag);
        if( sei_svc->layers[i].profile_level_info_present_flag )
        {
            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].layer_profile_level_idc = bs_read_u(b, 24); printf("sei_svc->layers[i].layer_profile_level_idc: %d \n", sei_svc->layers[i].layer_profile_level_idc);
        }
        if( sei_svc->layers[i].bitrate_info_present_flag )
        {
            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].avg_bitrate = bs_read_u(b, 16); printf("sei_svc->layers[i].avg_bitrate: %d \n", sei_svc->layers[i].avg_bitrate);
            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].max_bitrate_layer = bs_read_u(b, 16); printf("sei_svc->layers[i].max_bitrate_layer: %d \n", sei_svc->layers[i].max_bitrate_layer);
            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].max_bitrate_layer_representation = bs_read_u(b, 16); printf("sei_svc->layers[i].max_bitrate_layer_representation: %d \n", sei_svc->layers[i].max_bitrate_layer_representation);
            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].max_bitrate_calc_window = bs_read_u(b, 16); printf("sei_svc->layers[i].max_bitrate_calc_window: %d \n", sei_svc->layers[i].max_bitrate_calc_window);
        }
        if( sei_svc->layers[i].frm_rate_info_present_flag )
        {
            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].constant_frm_rate_idc = bs_read_u(b, 2); printf("sei_svc->layers[i].constant_frm_rate_idc: %d \n", sei_svc->layers[i].constant_frm_rate_idc);
            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].avg_frm_rate = bs_read_u(b, 16); printf("sei_svc->layers[i].avg_frm_rate: %d \n", sei_svc->layers[i].avg_frm_rate);
        }
        if( sei_svc->layers[i].frm_size_info_present_flag ||
            sei_svc->layers[i].iroi_division_info_present_flag )
        {
            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].frm_width_in_mbs_minus1 = bs_read_ue(b); printf("sei_svc->layers[i].frm_width_in_mbs_minus1: %d \n", sei_svc->layers[i].frm_width_in_mbs_minus1);
            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].frm_height_in_mbs_minus1 = bs_read_ue(b); printf("sei_svc->layers[i].frm_height_in_mbs_minus1: %d \n", sei_svc->layers[i].frm_height_in_mbs_minus1);
        }
        if( sei_svc->layers[i].sub_region_layer_flag )
        {
            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].base_region_layer_id = bs_read_ue(b); printf("sei_svc->layers[i].base_region_layer_id: %d \n", sei_svc->layers[i].base_region_layer_id);
            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].dynamic_rect_flag = bs_read_u1(b); printf("sei_svc->layers[i].dynamic_rect_flag: %d \n", sei_svc->layers[i].dynamic_rect_flag);
            if( sei_svc->layers[i].dynamic_rect_flag )
            {
                printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].horizontal_offset = bs_read_u(b, 16); printf("sei_svc->layers[i].horizontal_offset: %d \n", sei_svc->layers[i].horizontal_offset);
                printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].vertical_offset = bs_read_u(b, 16); printf("sei_svc->layers[i].vertical_offset: %d \n", sei_svc->layers[i].vertical_offset);
                printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].region_width = bs_read_u(b, 16); printf("sei_svc->layers[i].region_width: %d \n", sei_svc->layers[i].region_width);
                printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].region_height = bs_read_u(b, 16); printf("sei_svc->layers[i].region_height: %d \n", sei_svc->layers[i].region_height);
            }
        }
        if( sei_svc->layers[i].sub_pic_layer_flag )
        {
            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].roi_id = bs_read_ue(b); printf("sei_svc->layers[i].roi_id: %d \n", sei_svc->layers[i].roi_id);
        }
        if( sei_svc->layers[i].iroi_division_info_present_flag )
        {
            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].iroi_grid_flag = bs_read_u1(b); printf("sei_svc->layers[i].iroi_grid_flag: %d \n", sei_svc->layers[i].iroi_grid_flag);
            if( sei_svc->layers[i].iroi_grid_flag )
            {
                printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].grid_width_in_mbs_minus1 = bs_read_ue(b); printf("sei_svc->layers[i].grid_width_in_mbs_minus1: %d \n", sei_svc->layers[i].grid_width_in_mbs_minus1);
                printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].grid_height_in_mbs_minus1 = bs_read_ue(b); printf("sei_svc->layers[i].grid_height_in_mbs_minus1: %d \n", sei_svc->layers[i].grid_height_in_mbs_minus1);
            }
            else
            {
                printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].num_rois_minus1 = bs_read_ue(b); printf("sei_svc->layers[i].num_rois_minus1: %d \n", sei_svc->layers[i].num_rois_minus1);
                
                for( int j = 0; j <= sei_svc->layers[i].num_rois_minus1; j++ )
                {
                    printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].roi[j].first_mb_in_roi = bs_read_ue(b); printf("sei_svc->layers[i].roi[j].first_mb_in_roi: %d \n", sei_svc->layers[i].roi[j].first_mb_in_roi);
                    printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].roi[j].roi_width_in_mbs_minus1 = bs_read_ue(b); printf("sei_svc->layers[i].roi[j].roi_width_in_mbs_minus1: %d \n", sei_svc->layers[i].roi[j].roi_width_in_mbs_minus1);
                    printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].roi[j].roi_height_in_mbs_minus1 = bs_read_ue(b); printf("sei_svc->layers[i].roi[j].roi_height_in_mbs_minus1: %d \n", sei_svc->layers[i].roi[j].roi_height_in_mbs_minus1);
                }
            }
        }
        if( sei_svc->layers[i].layer_dependency_info_present_flag )
        {
            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].num_directly_dependent_layers = bs_read_ue(b); printf("sei_svc->layers[i].num_directly_dependent_layers: %d \n", sei_svc->layers[i].num_directly_dependent_layers);
            for( int j = 0; j < sei_svc->layers[i].num_directly_dependent_layers; j++ )
            {
                printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].directly_dependent_layer_id_delta_minus1[j] = bs_read_ue(b); printf("sei_svc->layers[i].directly_dependent_layer_id_delta_minus1[j]: %d \n", sei_svc->layers[i].directly_dependent_layer_id_delta_minus1[j]);
            }
        }
        else
        {
            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].layer_dependency_info_src_layer_id_delta = bs_read_ue(b); printf("sei_svc->layers[i].layer_dependency_info_src_layer_id_delta: %d \n", sei_svc->layers[i].layer_dependency_info_src_layer_id_delta);
        }
        if( sei_svc->layers[i].parameter_sets_info_present_flag )
        {
            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].num_seq_parameter_sets = bs_read_ue(b); printf("sei_svc->layers[i].num_seq_parameter_sets: %d \n", sei_svc->layers[i].num_seq_parameter_sets);
            for( int j = 0; j < sei_svc->layers[i].num_seq_parameter_sets; j++ )
            {
                printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].seq_parameter_set_id_delta[j] = bs_read_ue(b); printf("sei_svc->layers[i].seq_parameter_set_id_delta[j]: %d \n", sei_svc->layers[i].seq_parameter_set_id_delta[j]);
            }
            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].num_subset_seq_parameter_sets = bs_read_ue(b); printf("sei_svc->layers[i].num_subset_seq_parameter_sets: %d \n", sei_svc->layers[i].num_subset_seq_parameter_sets);
            for( int j = 0; j < sei_svc->layers[i].num_subset_seq_parameter_sets; j++ )
            {
                printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].subset_seq_parameter_set_id_delta[j] = bs_read_ue(b); printf("sei_svc->layers[i].subset_seq_parameter_set_id_delta[j]: %d \n", sei_svc->layers[i].subset_seq_parameter_set_id_delta[j]);
            }
            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].num_pic_parameter_sets_minus1 = bs_read_ue(b); printf("sei_svc->layers[i].num_pic_parameter_sets_minus1: %d \n", sei_svc->layers[i].num_pic_parameter_sets_minus1);
            for( int j = 0; j < sei_svc->layers[i].num_pic_parameter_sets_minus1; j++ )
            {
                printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].pic_parameter_set_id_delta[j] = bs_read_ue(b); printf("sei_svc->layers[i].pic_parameter_set_id_delta[j]: %d \n", sei_svc->layers[i].pic_parameter_set_id_delta[j]);
            }
        }
        else
        {
            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].parameter_sets_info_src_layer_id_delta = bs_read_ue(b); printf("sei_svc->layers[i].parameter_sets_info_src_layer_id_delta: %d \n", sei_svc->layers[i].parameter_sets_info_src_layer_id_delta);
        }
        if( sei_svc->layers[i].bitstream_restriction_info_present_flag )
        {
            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].motion_vectors_over_pic_boundaries_flag = bs_read_u1(b); printf("sei_svc->layers[i].motion_vectors_over_pic_boundaries_flag: %d \n", sei_svc->layers[i].motion_vectors_over_pic_boundaries_flag);
            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].max_bytes_per_pic_denom = bs_read_ue(b); printf("sei_svc->layers[i].max_bytes_per_pic_denom: %d \n", sei_svc->layers[i].max_bytes_per_pic_denom);
            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].max_bits_per_mb_denom = bs_read_ue(b); printf("sei_svc->layers[i].max_bits_per_mb_denom: %d \n", sei_svc->layers[i].max_bits_per_mb_denom);
            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].log2_max_mv_length_horizontal = bs_read_ue(b); printf("sei_svc->layers[i].log2_max_mv_length_horizontal: %d \n", sei_svc->layers[i].log2_max_mv_length_horizontal);
            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].log2_max_mv_length_vertical = bs_read_ue(b); printf("sei_svc->layers[i].log2_max_mv_length_vertical: %d \n", sei_svc->layers[i].log2_max_mv_length_vertical);
            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].max_num_reorder_frames = bs_read_ue(b); printf("sei_svc->layers[i].max_num_reorder_frames: %d \n", sei_svc->layers[i].max_num_reorder_frames);
            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].max_dec_frame_buffering = bs_read_ue(b); printf("sei_svc->layers[i].max_dec_frame_buffering: %d \n", sei_svc->layers[i].max_dec_frame_buffering);
        }
        if( sei_svc->layers[i].layer_conversion_flag )
        {
            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].conversion_type_idc = bs_read_ue(b); printf("sei_svc->layers[i].conversion_type_idc: %d \n", sei_svc->layers[i].conversion_type_idc);
            for( int j = 0; j < 2; j++ )
            {
                printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].rewriting_info_flag[j] = bs_read_u(b, 1); printf("sei_svc->layers[i].rewriting_info_flag[j]: %d \n", sei_svc->layers[i].rewriting_info_flag[j]);
                if( sei_svc->layers[i].rewriting_info_flag[j] )
                {
                    printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].rewriting_profile_level_idc[j] = bs_read_u(b, 24); printf("sei_svc->layers[i].rewriting_profile_level_idc[j]: %d \n", sei_svc->layers[i].rewriting_profile_level_idc[j]);
                    printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].rewriting_avg_bitrate[j] = bs_read_u(b, 16); printf("sei_svc->layers[i].rewriting_avg_bitrate[j]: %d \n", sei_svc->layers[i].rewriting_avg_bitrate[j]);
                    printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->layers[i].rewriting_max_bitrate[j] = bs_read_u(b, 16); printf("sei_svc->layers[i].rewriting_max_bitrate[j]: %d \n", sei_svc->layers[i].rewriting_max_bitrate[j]);
                }
            }
        }
    }

    if( sei_svc->priority_layer_info_present_flag )
    {
        printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->pr_num_dIds_minus1 = bs_read_ue(b); printf("sei_svc->pr_num_dIds_minus1: %d \n", sei_svc->pr_num_dIds_minus1);
        
        for( int i = 0; i <= sei_svc->pr_num_dIds_minus1; i++ ) {
            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->pr[i].pr_dependency_id = bs_read_u(b, 3); printf("sei_svc->pr[i].pr_dependency_id: %d \n", sei_svc->pr[i].pr_dependency_id);
            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->pr[i].pr_num_minus1 = bs_read_ue(b); printf("sei_svc->pr[i].pr_num_minus1: %d \n", sei_svc->pr[i].pr_num_minus1);
            for( int j = 0; j <= sei_svc->pr[i].pr_num_minus1; j++ )
            {
                printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->pr[i].pr_info[j].pr_id = bs_read_ue(b); printf("sei_svc->pr[i].pr_info[j].pr_id: %d \n", sei_svc->pr[i].pr_info[j].pr_id);
                printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->pr[i].pr_info[j].pr_profile_level_idc = bs_read_u(b, 24); printf("sei_svc->pr[i].pr_info[j].pr_profile_level_idc: %d \n", sei_svc->pr[i].pr_info[j].pr_profile_level_idc);
                printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->pr[i].pr_info[j].pr_avg_bitrate = bs_read_u(b, 16); printf("sei_svc->pr[i].pr_info[j].pr_avg_bitrate: %d \n", sei_svc->pr[i].pr_info[j].pr_avg_bitrate);
                printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_svc->pr[i].pr_info[j].pr_max_bitrate = bs_read_u(b, 16); printf("sei_svc->pr[i].pr_info[j].pr_max_bitrate: %d \n", sei_svc->pr[i].pr_info[j].pr_max_bitrate);
            }
        }
        
    }

}

// Appendix D.1.3 Picture timing SEI message syntax
void read_debug_sei_picture_timing( h264_stream_t* h, bs_t* b )
{
    sei_picture_timing_t* sei_pic_timing = h->sei->sei_pic_timing;

    const hrd_t* hrd_params = get_hrd_params( h );

    if ( hrd_params )
    {
        printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_pic_timing->cpb_removal_delay = bs_read_u(b, hrd_params->cpb_removal_delay_length_minus1 + 1); printf("sei_pic_timing->cpb_removal_delay: %d \n", sei_pic_timing->cpb_removal_delay);
        printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_pic_timing->dpb_output_delay = bs_read_u(b, hrd_params->dpb_output_delay_length_minus1 + 1); printf("sei_pic_timing->dpb_output_delay: %d \n", sei_pic_timing->dpb_output_delay);
    }

    if ( h && h->sps && h->sps->vui_parameters_present_flag && h->sps->vui.pic_struct_present_flag )
    {
        printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_pic_timing->pic_struct = bs_read_u(b, 4); printf("sei_pic_timing->pic_struct: %d \n", sei_pic_timing->pic_struct);

        const int num_clock_ts = pic_struct_to_num_clock_ts[sei_pic_timing->pic_struct];

        for ( int i = 0; i < num_clock_ts; i++ )
        {
            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_pic_timing->clock_timestamps[i].clock_timestamp_flag = bs_read_u1(b); printf("sei_pic_timing->clock_timestamps[i].clock_timestamp_flag: %d \n", sei_pic_timing->clock_timestamps[i].clock_timestamp_flag);

            if ( sei_pic_timing->clock_timestamps[i].clock_timestamp_flag )
            {
                printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_pic_timing->clock_timestamps[i].ct_type = bs_read_u(b, 2); printf("sei_pic_timing->clock_timestamps[i].ct_type: %d \n", sei_pic_timing->clock_timestamps[i].ct_type);
                printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_pic_timing->clock_timestamps[i].nuit_field_based_flag = bs_read_u1(b); printf("sei_pic_timing->clock_timestamps[i].nuit_field_based_flag: %d \n", sei_pic_timing->clock_timestamps[i].nuit_field_based_flag);
                printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_pic_timing->clock_timestamps[i].counting_type = bs_read_u(b, 5); printf("sei_pic_timing->clock_timestamps[i].counting_type: %d \n", sei_pic_timing->clock_timestamps[i].counting_type);
                printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_pic_timing->clock_timestamps[i].full_timestamp_flag = bs_read_u1(b); printf("sei_pic_timing->clock_timestamps[i].full_timestamp_flag: %d \n", sei_pic_timing->clock_timestamps[i].full_timestamp_flag);
                printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_pic_timing->clock_timestamps[i].discontinuity_flag = bs_read_u1(b); printf("sei_pic_timing->clock_timestamps[i].discontinuity_flag: %d \n", sei_pic_timing->clock_timestamps[i].discontinuity_flag);
                printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_pic_timing->clock_timestamps[i].cnt_dropped_flag = bs_read_u1(b); printf("sei_pic_timing->clock_timestamps[i].cnt_dropped_flag: %d \n", sei_pic_timing->clock_timestamps[i].cnt_dropped_flag);
                printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_pic_timing->clock_timestamps[i].n_frames = bs_read_u8(b); printf("sei_pic_timing->clock_timestamps[i].n_frames: %d \n", sei_pic_timing->clock_timestamps[i].n_frames);

                if ( sei_pic_timing->clock_timestamps[i].full_timestamp_flag )
                {
                    printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_pic_timing->clock_timestamps[i].seconds_value = bs_read_u(b, 6); printf("sei_pic_timing->clock_timestamps[i].seconds_value: %d \n", sei_pic_timing->clock_timestamps[i].seconds_value);
                    printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_pic_timing->clock_timestamps[i].minutes_value = bs_read_u(b, 6); printf("sei_pic_timing->clock_timestamps[i].minutes_value: %d \n", sei_pic_timing->clock_timestamps[i].minutes_value);
                    printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_pic_timing->clock_timestamps[i].hours_value = bs_read_u(b, 5); printf("sei_pic_timing->clock_timestamps[i].hours_value: %d \n", sei_pic_timing->clock_timestamps[i].hours_value);
                }
                else
                {
                    printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_pic_timing->clock_timestamps[i].seconds_flag = bs_read_u1(b); printf("sei_pic_timing->clock_timestamps[i].seconds_flag: %d \n", sei_pic_timing->clock_timestamps[i].seconds_flag);

                    if ( sei_pic_timing->clock_timestamps[i].seconds_flag )
                    {
                        printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_pic_timing->clock_timestamps[i].seconds_value = bs_read_u(b, 6); printf("sei_pic_timing->clock_timestamps[i].seconds_value: %d \n", sei_pic_timing->clock_timestamps[i].seconds_value);
                        printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_pic_timing->clock_timestamps[i].minutes_flag = bs_read_u1(b); printf("sei_pic_timing->clock_timestamps[i].minutes_flag: %d \n", sei_pic_timing->clock_timestamps[i].minutes_flag);

                        if ( sei_pic_timing->clock_timestamps[i].minutes_flag )
                        {
                            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_pic_timing->clock_timestamps[i].minutes_value = bs_read_u(b, 6); printf("sei_pic_timing->clock_timestamps[i].minutes_value: %d \n", sei_pic_timing->clock_timestamps[i].minutes_value);
                            printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_pic_timing->clock_timestamps[i].hours_flag = bs_read_u1(b); printf("sei_pic_timing->clock_timestamps[i].hours_flag: %d \n", sei_pic_timing->clock_timestamps[i].hours_flag);

                            if ( sei_pic_timing->clock_timestamps[i].hours_flag )
                            {
                                printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_pic_timing->clock_timestamps[i].hours_value = bs_read_u(b, 5); printf("sei_pic_timing->clock_timestamps[i].hours_value: %d \n", sei_pic_timing->clock_timestamps[i].hours_value);
                            }
                        }
                    }
                }
            }

            const int time_offset_length = hrd_params ? hrd_params->time_offset_length : 24;

            if ( time_offset_length )
            {
                printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_pic_timing->clock_timestamps[i].time_offset = bs_read_i(b, time_offset_length); printf("sei_pic_timing->clock_timestamps[i].time_offset: %d \n", sei_pic_timing->clock_timestamps[i].time_offset);
            }
        }
    }
}

// Appendix D.1.2 Buffering period SEI message syntax
void read_debug_sei_buffering_period( h264_stream_t* h, bs_t* b )
{
    sei_buffering_period_t* sei_buffering_period = h->sei->sei_buffering_period;

    printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_buffering_period->sps_id = bs_read_ue(b); printf("sei_buffering_period->sps_id: %d \n", sei_buffering_period->sps_id);

    if ( h && h->sps )
    {
        if ( h->sps->vui_parameters_present_flag && h->sps->vui.nal_hrd_parameters_present_flag )
        {
            const int length = h->sps->hrd_nal.initial_cpb_removal_delay_length_minus1 + 1;

            for ( int SchedSelIdx = 0; SchedSelIdx <= h->sps->hrd_nal.cpb_cnt_minus1; SchedSelIdx++ )
            {
                printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_buffering_period->initial_cpb_removal_delay[ SchedSelIdx ] = bs_read_u(b, length); printf("sei_buffering_period->initial_cpb_removal_delay[ SchedSelIdx ]: %d \n", sei_buffering_period->initial_cpb_removal_delay[ SchedSelIdx ]);
                printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_buffering_period->initial_cpb_delay_offset[ SchedSelIdx ] = bs_read_u(b, length); printf("sei_buffering_period->initial_cpb_delay_offset[ SchedSelIdx ]: %d \n", sei_buffering_period->initial_cpb_delay_offset[ SchedSelIdx ]);
            }
        }

        if ( h->sps->vui_parameters_present_flag && h->sps->vui.vcl_hrd_parameters_present_flag )
        {
            const int length = h->sps->hrd_vcl.initial_cpb_removal_delay_length_minus1 + 1;

            for ( int SchedSelIdx = 0; SchedSelIdx <= h->sps->hrd_vcl.cpb_cnt_minus1; SchedSelIdx++ )
            {
                printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_buffering_period->initial_cpb_removal_delay[ SchedSelIdx ] = bs_read_u(b, length); printf("sei_buffering_period->initial_cpb_removal_delay[ SchedSelIdx ]: %d \n", sei_buffering_period->initial_cpb_removal_delay[ SchedSelIdx ]);
                printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_buffering_period->initial_cpb_delay_offset[ SchedSelIdx ] = bs_read_u(b, length); printf("sei_buffering_period->initial_cpb_delay_offset[ SchedSelIdx ]: %d \n", sei_buffering_period->initial_cpb_delay_offset[ SchedSelIdx ]);
            }
        }
    }
}

// Appendix D.1.8 Recovery point SEI message syntax
void read_debug_sei_recovery_point( h264_stream_t* h, bs_t* b )
{
    sei_recovery_point_t* sei_recovery_point = h->sei->sei_recovery_point;

    printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_recovery_point->recovery_frame_cnt = bs_read_ue(b); printf("sei_recovery_point->recovery_frame_cnt: %d \n", sei_recovery_point->recovery_frame_cnt);
    printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_recovery_point->exact_match_flag = bs_read_u1(b); printf("sei_recovery_point->exact_match_flag: %d \n", sei_recovery_point->exact_match_flag);
    printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_recovery_point->broken_link_flag = bs_read_u1(b); printf("sei_recovery_point->broken_link_flag: %d \n", sei_recovery_point->broken_link_flag);
    printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); sei_recovery_point->changing_slice_grp_idc = bs_read_u(b, 2); printf("sei_recovery_point->changing_slice_grp_idc: %d \n", sei_recovery_point->changing_slice_grp_idc);
}

// D.1 SEI payload syntax
void read_debug_sei_payload( h264_stream_t* h, bs_t* b )
{
    sei_t* s = h->sei;
    
    int i;
    switch( s->payloadType )
    {
        case SEI_TYPE_SCALABILITY_INFO:
            if( 1 )
            {
                s->sei_svc = (sei_scalability_info_t*)calloc( 1, sizeof(sei_scalability_info_t) );
            }
            read_debug_sei_scalability_info( h, b );
            break;
        case SEI_TYPE_PIC_TIMING:
            if ( 1 )
            {
                s->sei_pic_timing = (sei_picture_timing_t*)calloc( 1, sizeof(sei_picture_timing_t) );
            }
            read_debug_sei_picture_timing( h, b );
            break;
        case SEI_TYPE_BUFFERING_PERIOD:
            if ( 1 )
            {
                s->sei_buffering_period = (sei_buffering_period_t*)calloc( 1, sizeof(sei_buffering_period_t) );
            }
            read_debug_sei_buffering_period( h, b );
            break;
        case SEI_TYPE_RECOVERY_POINT:
            if ( 1 )
            {
                s->sei_recovery_point = (sei_recovery_point_t*)calloc( 1, sizeof(sei_recovery_point_t) );
            }
            read_debug_sei_recovery_point( h, b );
            break;
        default:
            if( 1 )
            {
                s->data = (uint8_t*)calloc(1, s->payloadSize);
            }
            
            for ( i = 0; i < s->payloadSize; i++ )
            {
                printf("%ld.%d: ", (long int)(b->p - b->start), b->bits_left); s->data[i] = bs_read_u8(b); printf("s->data[i]: %d \n", s->data[i]);
            }
    }

    if( 1 )
        read_sei_end_bits(h, b);
}
