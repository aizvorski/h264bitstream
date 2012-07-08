/* 
 * h264bitstream - a library for reading and writing H.264 video
 * Copyright (C) 2005-2007 Auroras Entertainment, LLC
 * Copyright (C) 2008-2011 Avail-TVN
 * Copyright (C) 2012 Alex Izvorski
 *
 * Written by Alex Izvorski <aizvorski@gmail.com> and Alex Giladi <alex.giladi@gmail.com>
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

FILE* h264_dbgfile = NULL;

#define printf(...) fprintf((h264_dbgfile == NULL ? stdout : h264_dbgfile), __VA_ARGS__)

/** 
 Calculate the log base 2 of the argument, rounded up. 
 Zero or negative arguments return zero 
 Idea from http://www.southwindsgames.com/blog/2009/01/19/fast-integer-log2-function-in-cc/
 */
int intlog2(int x)
{
    int log = 0;
    if (x < 0) { x = 0; }
    while ((x >> log) > 0)
    {
        log++;
    }
    if (log > 0 && x == 1<<(log-1)) { log--; }
    return log;
}

int is_slice_type(int slice_type, int cmp_type)
{
    if (slice_type >= 5) { slice_type -= 5; }
    if (cmp_type >= 5) { cmp_type -= 5; }
    if (slice_type == cmp_type) { return 1; }
    else { return 0; }
}

int more_rbsp_data(h264_stream_t* h, bs_t* b) 
{
    if ( bs_eof(b) ) { return 0; }
    if ( bs_peek_u1(b) == 1 ) { return 0; } // if next bit is 1, we've reached the stop bit
    return 1;
}

int more_rbsp_trailing_data(h264_stream_t* h, bs_t* b) { return !bs_eof(b); }


#end_preamble

#function_declarations

//7.3.2.1 Sequence parameter set RBSP syntax
void structure(seq_parameter_set_rbsp)(h264_stream_t* h, bs_t* b)
{
    int i;

    sps_t* sps = h->sps;
    if( is_reading )
    {
        memset(sps, 0, sizeof(sps_t));
        sps->chroma_format_idc = 1; 
    }
 
    value( sps->profile_idc, u8 );
    value( sps->constraint_set0_flag, u1 );
    value( sps->constraint_set1_flag, u1 );
    value( sps->constraint_set2_flag, u1 );
    value( sps->constraint_set3_flag, u1 );
    value( sps->constraint_set4_flag, u1 );
    value( sps->constraint_set5_flag, u1 );
    value( sps->reserved_zero_2bits, f(2, 0) );
    value( sps->level_idc, u8 );
    value( sps->seq_parameter_set_id, ue );

    if( sps->profile_idc == 100 || sps->profile_idc == 110 ||
        sps->profile_idc == 122 || sps->profile_idc == 144 )
    {
        value( sps->chroma_format_idc, ue );
        if( sps->chroma_format_idc == 3 )
        {
            value( sps->residual_colour_transform_flag, u1 );
        }
        value( sps->bit_depth_luma_minus8, ue );
        value( sps->bit_depth_chroma_minus8, ue );
        value( sps->qpprime_y_zero_transform_bypass_flag, u1 );
        value( sps->seq_scaling_matrix_present_flag, u1 );
        if( sps->seq_scaling_matrix_present_flag )
        {
            for( i = 0; i < 8; i++ )
            {
                value( sps->seq_scaling_list_present_flag[ i ], u1 );
                if( sps->seq_scaling_list_present_flag[ i ] )
                {
                    if( i < 6 )
                    {
                        structure(scaling_list)( b, sps->ScalingList4x4[ i ], 16,
                                      sps->UseDefaultScalingMatrix4x4Flag[ i ]);
                    }
                    else
                    {
                        structure(scaling_list)( b, sps->ScalingList8x8[ i - 6 ], 64,
                                      sps->UseDefaultScalingMatrix8x8Flag[ i - 6 ] );
                    }
                }
            }
        }
    }
    value( sps->log2_max_frame_num_minus4, ue );
    value( sps->pic_order_cnt_type, ue );
    if( sps->pic_order_cnt_type == 0 )
    {
        value( sps->log2_max_pic_order_cnt_lsb_minus4, ue );
    }
    else if( sps->pic_order_cnt_type == 1 )
    {
        value( sps->delta_pic_order_always_zero_flag, u1 );
        value( sps->offset_for_non_ref_pic, se );
        value( sps->offset_for_top_to_bottom_field, se );
        value( sps->num_ref_frames_in_pic_order_cnt_cycle, ue );
        for( i = 0; i < sps->num_ref_frames_in_pic_order_cnt_cycle; i++ )
        {
            value( sps->offset_for_ref_frame[ i ], se );
        }
    }
    value( sps->num_ref_frames, ue );
    value( sps->gaps_in_frame_num_value_allowed_flag, u1 );
    value( sps->pic_width_in_mbs_minus1, ue );
    value( sps->pic_height_in_map_units_minus1, ue );
    value( sps->frame_mbs_only_flag, u1 );
    if( !sps->frame_mbs_only_flag )
    {
        value( sps->mb_adaptive_frame_field_flag, u1 );
    }
    value( sps->direct_8x8_inference_flag, u1 );
    value( sps->frame_cropping_flag, u1 );
    if( sps->frame_cropping_flag )
    {
        value( sps->frame_crop_left_offset, ue );
        value( sps->frame_crop_right_offset, ue );
        value( sps->frame_crop_top_offset, ue );
        value( sps->frame_crop_bottom_offset, ue );
    }
    value( sps->vui_parameters_present_flag, u1 );
    if( sps->vui_parameters_present_flag )
    {
        structure(vui_parameters)(h, b);
    }
    structure(rbsp_trailing_bits)(h, b);

    if( is_reading )
    {
        memcpy(sps, h->sps_table[sps->seq_parameter_set_id], sizeof(sps_t));
    }
}


//7.3.2.1.1 Scaling list syntax
void structure(scaling_list)(bs_t* b, int* scalingList, int sizeOfScalingList, int useDefaultScalingMatrixFlag )
{
    int lastScale = 8;
    int nextScale = 8;
    for( int j = 0; j < sizeOfScalingList; j++ )
    {
        if( nextScale != 0 )
        {
            int delta_scale;
            value( delta_scale, se );
            nextScale = ( lastScale + delta_scale + 256 ) % 256;
            useDefaultScalingMatrixFlag = ( j == 0 && nextScale == 0 );
        }
        scalingList[ j ] = ( nextScale == 0 ) ? lastScale : nextScale;
        lastScale = scalingList[ j ];
    }
}

//Appendix E.1.1 VUI parameters syntax
void structure(vui_parameters)(h264_stream_t* h, bs_t* b)
{
    sps_t* sps = h->sps;

    value( sps->vui.aspect_ratio_info_present_flag, u1 );
    if( sps->vui.aspect_ratio_info_present_flag )
    {
        value( sps->vui.aspect_ratio_idc, u8 );
        if( sps->vui.aspect_ratio_idc == SAR_Extended )
        {
            value( sps->vui.sar_width, u(16) );
            value( sps->vui.sar_height, u(16) );
        }
    }
    value( sps->vui.overscan_info_present_flag, u1 );
    if( sps->vui.overscan_info_present_flag )
    {
        value( sps->vui.overscan_appropriate_flag, u1 );
    }
    value( sps->vui.video_signal_type_present_flag, u1 );
    if( sps->vui.video_signal_type_present_flag )
    {
        value( sps->vui.video_format, u(3) );
        value( sps->vui.video_full_range_flag, u1 );
        value( sps->vui.colour_description_present_flag, u1 );
        if( sps->vui.colour_description_present_flag )
        {
            value( sps->vui.colour_primaries, u8 );
            value( sps->vui.transfer_characteristics, u8 );
            value( sps->vui.matrix_coefficients, u8 );
        }
    }
    value( sps->vui.chroma_loc_info_present_flag, u1 );
    if( sps->vui.chroma_loc_info_present_flag )
    {
        value( sps->vui.chroma_sample_loc_type_top_field, ue );
        value( sps->vui.chroma_sample_loc_type_bottom_field, ue );
    }
    value( sps->vui.timing_info_present_flag, u1 );
    if( sps->vui.timing_info_present_flag )
    {
        value( sps->vui.num_units_in_tick, u(32) );
        value( sps->vui.time_scale, u(32) );
        value( sps->vui.fixed_frame_rate_flag, u1 );
    }
    value( sps->vui.nal_hrd_parameters_present_flag, u1 );
    if( sps->vui.nal_hrd_parameters_present_flag )
    {
        structure(hrd_parameters)(h, b);
    }
    value( sps->vui.vcl_hrd_parameters_present_flag, u1 );
    if( sps->vui.vcl_hrd_parameters_present_flag )
    {
        structure(hrd_parameters)(h, b);
    }
    if( sps->vui.nal_hrd_parameters_present_flag || sps->vui.vcl_hrd_parameters_present_flag )
    {
        value( sps->vui.low_delay_hrd_flag, u1 );
    }
    value( sps->vui.pic_struct_present_flag, u1 );
    value( sps->vui.bitstream_restriction_flag, u1 );
    if( sps->vui.bitstream_restriction_flag )
    {
        value( sps->vui.motion_vectors_over_pic_boundaries_flag, u1 );
        value( sps->vui.max_bytes_per_pic_denom, ue );
        value( sps->vui.max_bits_per_mb_denom, ue );
        value( sps->vui.log2_max_mv_length_horizontal, ue );
        value( sps->vui.log2_max_mv_length_vertical, ue );
        value( sps->vui.num_reorder_frames, ue );
        value( sps->vui.max_dec_frame_buffering, ue );
    }
}


//Appendix E.1.2 HRD parameters syntax
void structure(hrd_parameters)(h264_stream_t* h, bs_t* b)
{
    sps_t* sps = h->sps;
    int SchedSelIdx;

    value( sps->hrd.cpb_cnt_minus1, ue );
    value( sps->hrd.bit_rate_scale, u(4) );
    value( sps->hrd.cpb_size_scale, u(4) );
    for( SchedSelIdx = 0; SchedSelIdx <= sps->hrd.cpb_cnt_minus1; SchedSelIdx++ )
    {
        value( sps->hrd.bit_rate_value_minus1[ SchedSelIdx ], ue );
        value( sps->hrd.cpb_size_value_minus1[ SchedSelIdx ], ue );
        value( sps->hrd.cbr_flag[ SchedSelIdx ], u1 );
    }
    value( sps->hrd.initial_cpb_removal_delay_length_minus1, u(5) );
    value( sps->hrd.cpb_removal_delay_length_minus1, u(5) );
    value( sps->hrd.dpb_output_delay_length_minus1, u(5) );
    value( sps->hrd.time_offset_length, u(5) );
}


/*
UNIMPLEMENTED
//7.3.2.1.2 Sequence parameter set extension RBSP syntax
int structure(seq_parameter_set_extension_rbsp)(bs_t* b, sps_ext_t* sps_ext) {
    value( seq_parameter_set_id, ue );
    value( aux_format_idc, ue );
    if( aux_format_idc != 0 ) {
        value( bit_depth_aux_minus8, ue );
        value( alpha_incr_flag, u1 );
        alpha_opaque_value = bs_structure(u)(v);
        alpha_transparent_value = bs_structure(u)(v);
    }
    value( additional_extension_flag, u1 );
    structure(rbsp_trailing_bits)();
}
*/

//7.3.2.2 Picture parameter set RBSP syntax
void structure(pic_parameter_set_rbsp)(h264_stream_t* h, bs_t* b)
{
    pps_t* pps = h->pps;
    if( is_reading )
    {
        memset(pps, 0, sizeof(pps_t));
    }

    int i;
    int i_group;

    value( pps->pic_parameter_set_id, ue);
    value( pps->seq_parameter_set_id, ue );
    value( pps->entropy_coding_mode_flag, u1 );
    value( pps->pic_order_present_flag, u1 );
    value( pps->num_slice_groups_minus1, ue );

    if( pps->num_slice_groups_minus1 > 0 )
    {
        value( pps->slice_group_map_type, ue );
        if( pps->slice_group_map_type == 0 )
        {
            for( i_group = 0; i_group <= pps->num_slice_groups_minus1; i_group++ )
            {
                value( pps->run_length_minus1[ i_group ], ue );
            }
        }
        else if( pps->slice_group_map_type == 2 )
        {
            for( i_group = 0; i_group < pps->num_slice_groups_minus1; i_group++ )
            {
                value( pps->top_left[ i_group ], ue );
                value( pps->bottom_right[ i_group ], ue );
            }
        }
        else if( pps->slice_group_map_type == 3 ||
                 pps->slice_group_map_type == 4 ||
                 pps->slice_group_map_type == 5 )
        {
            value( pps->slice_group_change_direction_flag, u1 );
            value( pps->slice_group_change_rate_minus1, ue );
        }
        else if( pps->slice_group_map_type == 6 )
        {
            value( pps->pic_size_in_map_units_minus1, ue );
            for( i = 0; i <= pps->pic_size_in_map_units_minus1; i++ )
            {
                int v = intlog2( pps->num_slice_groups_minus1 + 1 );
                value( pps->slice_group_id[ i ], u(v) );
            }
        }
    }
    value( pps->num_ref_idx_l0_active_minus1, ue );
    value( pps->num_ref_idx_l1_active_minus1, ue );
    value( pps->weighted_pred_flag, u1 );
    value( pps->weighted_bipred_idc, u(2) );
    value( pps->pic_init_qp_minus26, se );
    value( pps->pic_init_qs_minus26, se );
    value( pps->chroma_qp_index_offset, se );
    value( pps->deblocking_filter_control_present_flag, u1 );
    value( pps->constrained_intra_pred_flag, u1 );
    value( pps->redundant_pic_cnt_present_flag, u1 );

    // TODO read/write diff
    pps->_more_rbsp_data_present = more_rbsp_data(h, b);
    if( pps->_more_rbsp_data_present )
    {
        value( pps->transform_8x8_mode_flag, u1 );
        value( pps->pic_scaling_matrix_present_flag, u1 );
        if( pps->pic_scaling_matrix_present_flag )
        {
            for( i = 0; i < 6 + 2* pps->transform_8x8_mode_flag; i++ )
            {
                value( pps->pic_scaling_list_present_flag[ i ], u1 );
                if( pps->pic_scaling_list_present_flag[ i ] )
                {
                    if( i < 6 )
                    {
                        structure(scaling_list)( b, pps->ScalingList4x4[ i ], 16,
                                      pps->UseDefaultScalingMatrix4x4Flag[ i ] );
                    }
                    else
                    {
                        structure(scaling_list)( b, pps->ScalingList8x8[ i - 6 ], 64,
                                      pps->UseDefaultScalingMatrix8x8Flag[ i - 6 ] );
                    }
                }
            }
        }
        value( pps->second_chroma_qp_index_offset, se );
    }
    structure(rbsp_trailing_bits)(h, b);

    if( is_reading )
    {
        memcpy(pps, h->pps_table[pps->pic_parameter_set_id], sizeof(pps_t));
    }
}

//7.3.2.3 Supplemental enhancement information RBSP syntax
void structure(sei_rbsp)(h264_stream_t* h, bs_t* b)
{
    int i;
    for (i = 0; i < h->num_seis; i++)
    {
        sei_free(h->seis[i]);
    }
    
    h->num_seis = 0;
    do {
        h->num_seis++;
        h->seis = (sei_t**)realloc(h->seis, h->num_seis * sizeof(sei_t*));
        h->seis[h->num_seis - 1] = sei_new();
        h->sei = h->seis[h->num_seis - 1];
        structure(sei_message)(h, b);
    } while( more_rbsp_data(h, b) );
    structure(rbsp_trailing_bits)(h, b);
}

int structure(ff_coded_number)(bs_t* b)
{
    int n1 = 0;
    int n2;
    do 
    {
        value( n2, u8 );
        n1 += n2;
    } while (n2 == 0xff);
    return n1;
}

//7.3.2.3.1 Supplemental enhancement information message syntax
void structure(sei_message)(h264_stream_t* h, bs_t* b)
{
    h->sei->payloadType = structure(ff_coded_number)(b);
    h->sei->payloadSize = structure(ff_coded_number)(b);
    structure(sei_payload)( h, b, h->sei->payloadType, h->sei->payloadSize );
}

//7.3.2.4 Access unit delimiter RBSP syntax
void structure(access_unit_delimiter_rbsp)(h264_stream_t* h, bs_t* b)
{
    value( h->aud->primary_pic_type, u(3) );
    structure(rbsp_trailing_bits)(h, b);
}

//7.3.2.5 End of sequence RBSP syntax
void structure(end_of_seq_rbsp)(h264_stream_t* h, bs_t* b)
{
}

//7.3.2.6 End of stream RBSP syntax
void structure(end_of_stream_rbsp)(h264_stream_t* h, bs_t* b)
{
}

//7.3.2.7 Filler data RBSP syntax
void structure(filler_data_rbsp)(h264_stream_t* h, bs_t* b)
{
    while( bs_next_bits(b, 8) == 0xFF )
    {
        value( ff_byte, f(8, 0xFF) );
    }
    structure(rbsp_trailing_bits)(h, b);
}

//7.3.2.8 Slice layer without partitioning RBSP syntax
void structure(slice_layer_rbsp)(h264_stream_t* h,  bs_t* b)
{
    structure(slice_header)(h, b);
    slice_data_rbsp_t* slice_data = h->slice_data;

    if ( slice_data != NULL )
    {
        if ( slice_data->rbsp_buf != NULL ) free( slice_data->rbsp_buf ); 
        uint8_t *sptr = b->p + (!!b->bits_left); // CABAC-specific: skip alignment bits, if there are any
        slice_data->rbsp_size = b->end - sptr;
        
        slice_data->rbsp_buf = (uint8_t*)malloc(slice_data->rbsp_size);
        memcpy( slice_data->rbsp_buf, sptr, slice_data->rbsp_size );
        // ugly hack: since next NALU starts at byte border, we are going to be padded by trailing_bits;
        return;
    }

    // FIXME should read or skip data
    //slice_data( ); /* all categories of slice_data( ) syntax */
    structure(rbsp_slice_trailing_bits)(h, b);
}

/*
// UNIMPLEMENTED
//7.3.2.9.1 Slice data partition A RBSP syntax
slice_data_partition_a_layer_rbsp( ) {
    structure(slice_header)( );             // only category 2
    slice_id = bs_structure(ue)(b)
    structure(slice_data)( );               // only category 2
    structure(rbsp_slice_trailing_bits)( ); // only category 2
}

//7.3.2.9.2 Slice data partition B RBSP syntax
slice_data_partition_b_layer_rbsp( ) {
    value( slice_id, ue );    // only category 3
    if( redundant_pic_cnt_present_flag )
        value( redundant_pic_cnt, ue );
    structure(slice_data)( );               // only category 3
    structure(rbsp_slice_trailing_bits)( ); // only category 3
}

//7.3.2.9.3 Slice data partition C RBSP syntax
slice_data_partition_c_layer_rbsp( ) {
    value( slice_id, ue );    // only category 4
    if( redundant_pic_cnt_present_flag )
        value( redundant_pic_cnt, ue );
    structure(slice_data)( );               // only category 4
    rbsp_slice_trailing_bits( ); // only category 4
}
*/

//7.3.2.10 RBSP slice trailing bits syntax
void structure(rbsp_slice_trailing_bits)(h264_stream_t* h, bs_t* b)
{
    structure(rbsp_trailing_bits)(h, b);
    int cabac_zero_word;
    if( h->pps->entropy_coding_mode_flag )
    {
        while( more_rbsp_trailing_data(h, b) )
        {
            value( cabac_zero_word, f(16, 0x0000) );
        }
    }
}

//7.3.2.11 RBSP trailing bits syntax
void structure(rbsp_trailing_bits)(h264_stream_t* h, bs_t* b)
{
    value( rbsp_stop_one_bit, f(1, 1) );

    while( !bs_byte_aligned(b) )
    {
        value( rbsp_alignment_zero_bit, f(1, 0) );
    }
}

//7.3.3 Slice header syntax
void structure(slice_header)(h264_stream_t* h, bs_t* b)
{
    slice_header_t* sh = h->sh;
    if( is_reading )
    {
        memset(sh, 0, sizeof(slice_header_t));
    }

    nal_t* nal = h->nal;

    value( sh->first_mb_in_slice, ue );
    value( sh->slice_type, ue );
    value( sh->pic_parameter_set_id, ue );

    // TODO check existence, otherwise fail
    pps_t* pps = h->pps = h->pps_table[sh->pic_parameter_set_id];
    sps_t* sps = h->sps = h->sps_table[pps->seq_parameter_set_id];

    value( sh->frame_num, u(sps->log2_max_frame_num_minus4 + 4 ) ); // was u(v)
    if( !sps->frame_mbs_only_flag )
    {
        value( sh->field_pic_flag, u1 );
        if( sh->field_pic_flag )
        {
            value( sh->bottom_field_flag, u1 );
        }
    }
    if( nal->nal_unit_type == 5 )
    {
        value( sh->idr_pic_id, ue );
    }
    if( sps->pic_order_cnt_type == 0 )
    {
        value( sh->pic_order_cnt_lsb, u(sps->log2_max_pic_order_cnt_lsb_minus4 + 4 ) ); // was u(v)
        if( pps->pic_order_present_flag && !sh->field_pic_flag )
        {
            value( sh->delta_pic_order_cnt_bottom, se );
        }
    }
    if( sps->pic_order_cnt_type == 1 && !sps->delta_pic_order_always_zero_flag )
    {
        value( sh->delta_pic_order_cnt[ 0 ], se );
        if( pps->pic_order_present_flag && !sh->field_pic_flag )
        {
            value( sh->delta_pic_order_cnt[ 1 ], se );
        }
    }
    if( pps->redundant_pic_cnt_present_flag )
    {
        value( sh->redundant_pic_cnt, ue );
    }
    if( is_slice_type( sh->slice_type, SH_SLICE_TYPE_B ) )
    {
        value( sh->direct_spatial_mv_pred_flag, u1 );
    }
    if( is_slice_type( sh->slice_type, SH_SLICE_TYPE_P ) || is_slice_type( sh->slice_type, SH_SLICE_TYPE_SP ) || is_slice_type( sh->slice_type, SH_SLICE_TYPE_B ) )
    {
        value( sh->num_ref_idx_active_override_flag, u1 );
        if( sh->num_ref_idx_active_override_flag )
        {
            value( sh->num_ref_idx_l0_active_minus1, ue ); // FIXME does this modify the pps?
            if( is_slice_type( sh->slice_type, SH_SLICE_TYPE_B ) )
            {
                value( sh->num_ref_idx_l1_active_minus1, ue );
            }
        }
    }
    structure(ref_pic_list_reordering)(h, b);
    if( ( pps->weighted_pred_flag && ( is_slice_type( sh->slice_type, SH_SLICE_TYPE_P ) || is_slice_type( sh->slice_type, SH_SLICE_TYPE_SP ) ) ) ||
        ( pps->weighted_bipred_idc == 1 && is_slice_type( sh->slice_type, SH_SLICE_TYPE_B ) ) )
    {
        structure(pred_weight_table)(h, b);
    }
    if( nal->nal_ref_idc != 0 )
    {
        structure(dec_ref_pic_marking)(h, b);
    }
    if( pps->entropy_coding_mode_flag && ! is_slice_type( sh->slice_type, SH_SLICE_TYPE_I ) && ! is_slice_type( sh->slice_type, SH_SLICE_TYPE_SI ) )
    {
        value( sh->cabac_init_idc, ue );
    }
    value( sh->slice_qp_delta, se );
    if( is_slice_type( sh->slice_type, SH_SLICE_TYPE_SP ) || is_slice_type( sh->slice_type, SH_SLICE_TYPE_SI ) )
    {
        if( is_slice_type( sh->slice_type, SH_SLICE_TYPE_SP ) )
        {
            value( sh->sp_for_switch_flag, u1 );
        }
        value( sh->slice_qs_delta, se );
    }
    if( pps->deblocking_filter_control_present_flag )
    {
        value( sh->disable_deblocking_filter_idc, ue );
        if( sh->disable_deblocking_filter_idc != 1 )
        {
            value( sh->slice_alpha_c0_offset_div2, se );
            value( sh->slice_beta_offset_div2, se );
        }
    }
    if( pps->num_slice_groups_minus1 > 0 &&
        pps->slice_group_map_type >= 3 && pps->slice_group_map_type <= 5)
    {
        int v = intlog2( pps->pic_size_in_map_units_minus1 +  pps->slice_group_change_rate_minus1 + 1 );
        value( sh->slice_group_change_cycle, u(v) ); // FIXME add 2?
    }
}

//7.3.3.1 Reference picture list reordering syntax
void structure(ref_pic_list_reordering)(h264_stream_t* h, bs_t* b)
{
    slice_header_t* sh = h->sh;
    // FIXME should be an array

    if( ! is_slice_type( sh->slice_type, SH_SLICE_TYPE_I ) && ! is_slice_type( sh->slice_type, SH_SLICE_TYPE_SI ) )
    {
        value( sh->rplr.ref_pic_list_reordering_flag_l0, u1 );
        if( sh->rplr.ref_pic_list_reordering_flag_l0 )
        {
            do
            {
                value( sh->rplr.reordering_of_pic_nums_idc, ue );
                if( sh->rplr.reordering_of_pic_nums_idc == 0 ||
                    sh->rplr.reordering_of_pic_nums_idc == 1 )
                {
                    value( sh->rplr.abs_diff_pic_num_minus1, ue );
                }
                else if( sh->rplr.reordering_of_pic_nums_idc == 2 )
                {
                    value( sh->rplr.long_term_pic_num, ue );
                }
            } while( sh->rplr.reordering_of_pic_nums_idc != 3 && ! bs_eof(b) );
        }
    }
    if( is_slice_type( sh->slice_type, SH_SLICE_TYPE_B ) )
    {
        value( sh->rplr.ref_pic_list_reordering_flag_l1, u1 );
        if( sh->rplr.ref_pic_list_reordering_flag_l1 )
        {
            do
            {
                value( sh->rplr.reordering_of_pic_nums_idc, ue );
                if( sh->rplr.reordering_of_pic_nums_idc == 0 ||
                    sh->rplr.reordering_of_pic_nums_idc == 1 )
                {
                    value( sh->rplr.abs_diff_pic_num_minus1, ue );
                }
                else if( sh->rplr.reordering_of_pic_nums_idc == 2 )
                {
                    value( sh->rplr.long_term_pic_num, ue );
                }
            } while( sh->rplr.reordering_of_pic_nums_idc != 3 && ! bs_eof(b) );
        }
    }
}

//7.3.3.2 Prediction weight table syntax
void structure(pred_weight_table)(h264_stream_t* h, bs_t* b)
{
    slice_header_t* sh = h->sh;
    sps_t* sps = h->sps;
    pps_t* pps = h->pps;

    int i, j;

    value( sh->pwt.luma_log2_weight_denom, ue );
    if( sps->chroma_format_idc != 0 )
    {
        value( sh->pwt.chroma_log2_weight_denom, ue );
    }
    for( i = 0; i <= pps->num_ref_idx_l0_active_minus1; i++ )
    {
        value( sh->pwt.luma_weight_l0_flag[i], u1 );
        if( sh->pwt.luma_weight_l0_flag[i] )
        {
            value( sh->pwt.luma_weight_l0[ i ], se );
            value( sh->pwt.luma_offset_l0[ i ], se );
        }
        if ( sps->chroma_format_idc != 0 )
        {
            value( sh->pwt.chroma_weight_l0_flag[i], u1 );
            if( sh->pwt.chroma_weight_l0_flag[i] )
            {
                for( j =0; j < 2; j++ )
                {
                    value( sh->pwt.chroma_weight_l0[ i ][ j ], se );
                    value( sh->pwt.chroma_offset_l0[ i ][ j ], se );
                }
            }
        }
    }
    if( is_slice_type( sh->slice_type, SH_SLICE_TYPE_B ) )
    {
        for( i = 0; i <= pps->num_ref_idx_l1_active_minus1; i++ )
        {
            value( sh->pwt.luma_weight_l1_flag[i], u1 );
            if( sh->pwt.luma_weight_l1_flag[i] )
            {
                value( sh->pwt.luma_weight_l1[ i ], se );
                value( sh->pwt.luma_offset_l1[ i ], se );
            }
            if( sps->chroma_format_idc != 0 )
            {
                value( sh->pwt.chroma_weight_l1_flag[i], u1 );
                if( sh->pwt.chroma_weight_l1_flag[i] )
                {
                    for( j = 0; j < 2; j++ )
                    {
                        value( sh->pwt.chroma_weight_l1[ i ][ j ], se );
                        value( sh->pwt.chroma_offset_l1[ i ][ j ], se );
                    }
                }
            }
        }
    }
}

//7.3.3.3 Decoded reference picture marking syntax
void structure(dec_ref_pic_marking)(h264_stream_t* h, bs_t* b)
{
    slice_header_t* sh = h->sh;
    // FIXME should be an array

    if( h->nal->nal_unit_type == 5 )
    {
        value( sh->drpm.no_output_of_prior_pics_flag, u1 );
        value( sh->drpm.long_term_reference_flag, u1 );
    }
    else
    {
        value( sh->drpm.adaptive_ref_pic_marking_mode_flag, u1 );
        if( sh->drpm.adaptive_ref_pic_marking_mode_flag )
        {
            do
            {
                value( sh->drpm.memory_management_control_operation, ue );
                if( sh->drpm.memory_management_control_operation == 1 ||
                    sh->drpm.memory_management_control_operation == 3 )
                {
                    value( sh->drpm.difference_of_pic_nums_minus1, ue );
                }
                if(sh->drpm.memory_management_control_operation == 2 )
                {
                    value( sh->drpm.long_term_pic_num, ue );
                }
                if( sh->drpm.memory_management_control_operation == 3 ||
                    sh->drpm.memory_management_control_operation == 6 )
                {
                    value( sh->drpm.long_term_frame_idx, ue );
                }
                if( sh->drpm.memory_management_control_operation == 4 )
                {
                    value( sh->drpm.max_long_term_frame_idx_plus1, ue );
                }
            } while( sh->drpm.memory_management_control_operation != 0 && ! bs_eof(b) );
        }
    }
}
