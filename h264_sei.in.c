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
#end_preamble

#function_declarations

// Appendix G.13.1.1 Scalability information SEI message syntax
void structure(sei_scalability_info)( h264_stream_t* h, bs_t* b )
{
    sei_scalability_info_t* sei_svc = h->sei->sei_svc;
    
    value( sei_svc->temporal_id_nesting_flag, u1 );
    value( sei_svc->priority_layer_info_present_flag, u1 );
    value( sei_svc->priority_id_setting_flag, u1 );
    value( sei_svc->num_layers_minus1, ue );
    
    for( int i = 0; i <= sei_svc->num_layers_minus1; i++ ) {
        value( sei_svc->layers[i].layer_id, ue );
        value( sei_svc->layers[i].priority_id, u(6) );
        value( sei_svc->layers[i].discardable_flag, u1 );
        value( sei_svc->layers[i].dependency_id, u(3) );
        value( sei_svc->layers[i].quality_id, u(4) );
        value( sei_svc->layers[i].temporal_id, u(3) );
        value( sei_svc->layers[i].sub_pic_layer_flag, u1 );
        value( sei_svc->layers[i].sub_region_layer_flag, u1 );
        value( sei_svc->layers[i].iroi_division_info_present_flag, u1 );
        value( sei_svc->layers[i].profile_level_info_present_flag, u1 );
        value( sei_svc->layers[i].bitrate_info_present_flag, u1 );
        value( sei_svc->layers[i].frm_rate_info_present_flag, u1 );
        value( sei_svc->layers[i].frm_size_info_present_flag, u1 );
        value( sei_svc->layers[i].layer_dependency_info_present_flag, u1 );
        value( sei_svc->layers[i].parameter_sets_info_present_flag, u1 );
        value( sei_svc->layers[i].bitstream_restriction_info_present_flag, u1 );
        value( sei_svc->layers[i].exact_inter_layer_pred_flag, u1 );
        if( sei_svc->layers[i].sub_pic_layer_flag ||
            sei_svc->layers[i].iroi_division_info_present_flag )
        {
            value( sei_svc->layers[i].exact_sample_value_match_flag, u1 );
        }
        value( sei_svc->layers[i].layer_conversion_flag, u1 );
        value( sei_svc->layers[i].layer_output_flag, u1 );
        if( sei_svc->layers[i].profile_level_info_present_flag )
        {
            value( sei_svc->layers[i].layer_profile_level_idc, u(24) );
        }
        if( sei_svc->layers[i].bitrate_info_present_flag )
        {
            value( sei_svc->layers[i].avg_bitrate, u(16) );
            value( sei_svc->layers[i].max_bitrate_layer, u(16) );
            value( sei_svc->layers[i].max_bitrate_layer_representation, u(16) );
            value( sei_svc->layers[i].max_bitrate_calc_window, u(16) );
        }
        if( sei_svc->layers[i].frm_rate_info_present_flag )
        {
            value( sei_svc->layers[i].constant_frm_rate_idc, u(2) );
            value( sei_svc->layers[i].avg_frm_rate, u(16) );
        }
        if( sei_svc->layers[i].frm_size_info_present_flag ||
            sei_svc->layers[i].iroi_division_info_present_flag )
        {
            value( sei_svc->layers[i].frm_width_in_mbs_minus1, ue );
            value( sei_svc->layers[i].frm_height_in_mbs_minus1, ue );
        }
        if( sei_svc->layers[i].sub_region_layer_flag )
        {
            value( sei_svc->layers[i].base_region_layer_id, ue );
            value( sei_svc->layers[i].dynamic_rect_flag, u1 );
            if( sei_svc->layers[i].dynamic_rect_flag )
            {
                value( sei_svc->layers[i].horizontal_offset, u(16) );
                value( sei_svc->layers[i].vertical_offset, u(16) );
                value( sei_svc->layers[i].region_width, u(16) );
                value( sei_svc->layers[i].region_height, u(16) );
            }
        }
        if( sei_svc->layers[i].sub_pic_layer_flag )
        {
            value( sei_svc->layers[i].roi_id, ue );
        }
        if( sei_svc->layers[i].iroi_division_info_present_flag )
        {
            value( sei_svc->layers[i].iroi_grid_flag, u1 );
            if( sei_svc->layers[i].iroi_grid_flag )
            {
                value( sei_svc->layers[i].grid_width_in_mbs_minus1, ue );
                value( sei_svc->layers[i].grid_height_in_mbs_minus1, ue );
            }
            else
            {
                value( sei_svc->layers[i].num_rois_minus1, ue );
                
                for( int j = 0; j <= sei_svc->layers[i].num_rois_minus1; j++ )
                {
                    value( sei_svc->layers[i].roi[j].first_mb_in_roi, ue );
                    value( sei_svc->layers[i].roi[j].roi_width_in_mbs_minus1, ue );
                    value( sei_svc->layers[i].roi[j].roi_height_in_mbs_minus1, ue );
                }
            }
        }
        if( sei_svc->layers[i].layer_dependency_info_present_flag )
        {
            value( sei_svc->layers[i].num_directly_dependent_layers, ue );
            for( int j = 0; j < sei_svc->layers[i].num_directly_dependent_layers; j++ )
            {
                value( sei_svc->layers[i].directly_dependent_layer_id_delta_minus1[j], ue );
            }
        }
        else
        {
            value( sei_svc->layers[i].layer_dependency_info_src_layer_id_delta, ue );
        }
        if( sei_svc->layers[i].parameter_sets_info_present_flag )
        {
            value( sei_svc->layers[i].num_seq_parameter_sets, ue );
            for( int j = 0; j < sei_svc->layers[i].num_seq_parameter_sets; j++ )
            {
                value( sei_svc->layers[i].seq_parameter_set_id_delta[j], ue );
            }
            value( sei_svc->layers[i].num_subset_seq_parameter_sets, ue );
            for( int j = 0; j < sei_svc->layers[i].num_subset_seq_parameter_sets; j++ )
            {
                value( sei_svc->layers[i].subset_seq_parameter_set_id_delta[j], ue );
            }
            value( sei_svc->layers[i].num_pic_parameter_sets_minus1, ue );
            for( int j = 0; j < sei_svc->layers[i].num_pic_parameter_sets_minus1; j++ )
            {
                value( sei_svc->layers[i].pic_parameter_set_id_delta[j], ue );
            }
        }
        else
        {
            value( sei_svc->layers[i].parameter_sets_info_src_layer_id_delta, ue );
        }
        if( sei_svc->layers[i].bitstream_restriction_info_present_flag )
        {
            value( sei_svc->layers[i].motion_vectors_over_pic_boundaries_flag, u1 );
            value( sei_svc->layers[i].max_bytes_per_pic_denom, ue );
            value( sei_svc->layers[i].max_bits_per_mb_denom, ue );
            value( sei_svc->layers[i].log2_max_mv_length_horizontal, ue );
            value( sei_svc->layers[i].log2_max_mv_length_vertical, ue );
            value( sei_svc->layers[i].max_num_reorder_frames, ue );
            value( sei_svc->layers[i].max_dec_frame_buffering, ue );
        }
        if( sei_svc->layers[i].layer_conversion_flag )
        {
            value( sei_svc->layers[i].conversion_type_idc, ue );
            for( int j = 0; j < 2; j++ )
            {
                value( sei_svc->layers[i].rewriting_info_flag[j], u(1) );
                if( sei_svc->layers[i].rewriting_info_flag[j] )
                {
                    value( sei_svc->layers[i].rewriting_profile_level_idc[j], u(24) );
                    value( sei_svc->layers[i].rewriting_avg_bitrate[j], u(16) );
                    value( sei_svc->layers[i].rewriting_max_bitrate[j], u(16) );
                }
            }
        }
    }

    if( sei_svc->priority_layer_info_present_flag )
    {
        value( sei_svc->pr_num_dIds_minus1, ue );
        
        for( int i = 0; i <= sei_svc->pr_num_dIds_minus1; i++ ) {
            value( sei_svc->pr[i].pr_dependency_id, u(3) );
            value( sei_svc->pr[i].pr_num_minus1, ue );
            for( int j = 0; j <= sei_svc->pr[i].pr_num_minus1; j++ )
            {
                value( sei_svc->pr[i].pr_info[j].pr_id, ue );
                value( sei_svc->pr[i].pr_info[j].pr_profile_level_idc, u(24) );
                value( sei_svc->pr[i].pr_info[j].pr_avg_bitrate, u(16) );
                value( sei_svc->pr[i].pr_info[j].pr_max_bitrate, u(16) );
            }
        }
        
    }

}

// Appendix D.1.3 Picture timing SEI message syntax
void structure(sei_picture_timing)( h264_stream_t* h, bs_t* b )
{
    sei_picture_timing_t* sei_pic_timing = h->sei->sei_pic_timing;

    const hrd_t* hrd_params = get_hrd_params( h );

    if ( hrd_params )
    {
        value( sei_pic_timing->cpb_removal_delay, u(hrd_params->cpb_removal_delay_length_minus1 + 1) );
        value( sei_pic_timing->dpb_output_delay, u(hrd_params->dpb_output_delay_length_minus1 + 1) );
    }

    if ( h && h->sps && h->sps->vui_parameters_present_flag && h->sps->vui.pic_struct_present_flag )
    {
        value( sei_pic_timing->pic_struct, u(4) );

        const int num_clock_ts = pic_struct_to_num_clock_ts[sei_pic_timing->pic_struct];

        for ( int i = 0; i < num_clock_ts; i++ )
        {
            value( sei_pic_timing->clock_timestamps[i].clock_timestamp_flag, u1 );

            if ( sei_pic_timing->clock_timestamps[i].clock_timestamp_flag )
            {
                value( sei_pic_timing->clock_timestamps[i].ct_type, _u(2) );
                value( sei_pic_timing->clock_timestamps[i].nuit_field_based_flag, u1 );
                value( sei_pic_timing->clock_timestamps[i].counting_type, u(5) );
                value( sei_pic_timing->clock_timestamps[i].full_timestamp_flag, u1 );
                value( sei_pic_timing->clock_timestamps[i].discontinuity_flag, u1 );
                value( sei_pic_timing->clock_timestamps[i].cnt_dropped_flag, u1 );
                value( sei_pic_timing->clock_timestamps[i].n_frames, u8 );

                if ( sei_pic_timing->clock_timestamps[i].full_timestamp_flag )
                {
                    value( sei_pic_timing->clock_timestamps[i].seconds_value, u(6) );
                    value( sei_pic_timing->clock_timestamps[i].minutes_value, u(6) );
                    value( sei_pic_timing->clock_timestamps[i].hours_value, u(5) );
                }
                else
                {
                    value( sei_pic_timing->clock_timestamps[i].seconds_flag, u1 );

                    if ( sei_pic_timing->clock_timestamps[i].seconds_flag )
                    {
                        value( sei_pic_timing->clock_timestamps[i].seconds_value, u(6) );
                        value( sei_pic_timing->clock_timestamps[i].minutes_flag, u1 );

                        if ( sei_pic_timing->clock_timestamps[i].minutes_flag )
                        {
                            value( sei_pic_timing->clock_timestamps[i].minutes_value, u(6) );
                            value( sei_pic_timing->clock_timestamps[i].hours_flag, u1 );

                            if ( sei_pic_timing->clock_timestamps[i].hours_flag )
                            {
                                value( sei_pic_timing->clock_timestamps[i].hours_value, u(5) );
                            }
                        }
                    }
                }
            }

            const int time_offset_length = hrd_params ? hrd_params->time_offset_length : 24;

            if ( time_offset_length )
            {
                value( sei_pic_timing->clock_timestamps[i].time_offset, i(time_offset_length) );
            }
        }
    }
}

// Appendix D.1.2 Buffering period SEI message syntax
void structure(sei_buffering_period)( h264_stream_t* h, bs_t* b )
{
    sei_buffering_period_t* sei_buffering_period = h->sei->sei_buffering_period;

    value( sei_buffering_period->sps_id, ue );

    if ( h && h->sps )
    {
        if ( h->sps->vui_parameters_present_flag && h->sps->vui.nal_hrd_parameters_present_flag )
        {
            const int length = h->sps->hrd_nal.initial_cpb_removal_delay_length_minus1 + 1;

            for ( int SchedSelIdx = 0; SchedSelIdx <= h->sps->hrd_nal.cpb_cnt_minus1; SchedSelIdx++ )
            {
                value( sei_buffering_period->initial_cpb_removal_delay[ SchedSelIdx ], u(length) );
                value( sei_buffering_period->initial_cpb_delay_offset[ SchedSelIdx ], u(length) );
            }
        }

        if ( h->sps->vui_parameters_present_flag && h->sps->vui.vcl_hrd_parameters_present_flag )
        {
            const int length = h->sps->hrd_vcl.initial_cpb_removal_delay_length_minus1 + 1;

            for ( int SchedSelIdx = 0; SchedSelIdx <= h->sps->hrd_vcl.cpb_cnt_minus1; SchedSelIdx++ )
            {
                value( sei_buffering_period->initial_cpb_removal_delay[ SchedSelIdx ], u(length) );
                value( sei_buffering_period->initial_cpb_delay_offset[ SchedSelIdx ], u(length) );
            }
        }
    }
}

// Appendix D.1.8 Recovery point SEI message syntax
void structure(sei_recovery_point)( h264_stream_t* h, bs_t* b )
{
    sei_recovery_point_t* sei_recovery_point = h->sei->sei_recovery_point;

    value( sei_recovery_point->recovery_frame_cnt, ue );
    value( sei_recovery_point->exact_match_flag, u1 );
    value( sei_recovery_point->broken_link_flag, u1 );
    value( sei_recovery_point->changing_slice_grp_idc, u(2) );
}

// D.1 SEI payload syntax
void structure(sei_payload)( h264_stream_t* h, bs_t* b )
{
    sei_t* s = h->sei;
    
    int i;
    switch( s->payloadType )
    {
        case SEI_TYPE_SCALABILITY_INFO:
            if( is_reading )
            {
                s->sei_svc = (sei_scalability_info_t*)calloc( 1, sizeof(sei_scalability_info_t) );
            }
            structure(sei_scalability_info)( h, b );
            break;
        case SEI_TYPE_PIC_TIMING:
            if ( is_reading )
            {
                s->sei_pic_timing = (sei_picture_timing_t*)calloc( 1, sizeof(sei_picture_timing_t) );
            }
            structure(sei_picture_timing)( h, b );
            break;
        case SEI_TYPE_BUFFERING_PERIOD:
            if ( is_reading )
            {
                s->sei_buffering_period = (sei_buffering_period_t*)calloc( 1, sizeof(sei_buffering_period_t) );
            }
            structure(sei_buffering_period)( h, b );
            break;
        case SEI_TYPE_RECOVERY_POINT:
            if ( is_reading )
            {
                s->sei_recovery_point = (sei_recovery_point_t*)calloc( 1, sizeof(sei_recovery_point_t) );
            }
            structure(sei_recovery_point)( h, b );
            break;
        default:
            if( is_reading )
            {
                s->data = (uint8_t*)calloc(1, s->payloadSize);
            }
            
            for ( i = 0; i < s->payloadSize; i++ )
            {
                value( s->data[i], u8 );
            }
    }

    if( is_reading )
        read_sei_end_bits(h, b);
}
