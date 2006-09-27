/* 
 * h264bitstream - a library for reading and writing H.264 video
 * Copyright (C) 2005-2006 Auroras Entertainment, LLC
 * 
 * Written by Alex Izvorski <aizvorski@gmail.com>
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
#include <math.h>

#include "bs.h"
#include "h264_stream.h"


#define log2(x) ( (1/log(2)) * log( (x) ) )


/****** reading ******/

/**
 Create a new H264 stream object.  Allocates all structures contained within it.
 @return    the stream object
 */
h264_stream_t* h264_new()
{
    h264_stream_t* h = (h264_stream_t*)malloc(sizeof(h264_stream_t));
    h->nal = (nal_t*)malloc(sizeof(nal_t));
    h->sps = (sps_t*)malloc(sizeof(sps_t));
    h->pps = (pps_t*)malloc(sizeof(pps_t));
    h->sh = (slice_header_t*)malloc(sizeof(slice_header_t));
    return h;
}


/**
 Free an existing H264 stream object.  Frees all contained structures.
 @param[in,out] h   the stream object
 */
void h264_free(h264_stream_t* h)
{
    free(h->nal);
    free(h->sps);
    free(h->pps);
    free(h->sh);
    free(h);
}

/**
 Find the beginning and end of a NAL (Network Abstraction Layer) unit in a byte buffer containing H264 bitstream data.
 @param[in]   buf        the buffer
 @param[in]   size       the size of the buffer
 @param[out]  nal_start  the beginning offset of the nal
 @param[out]  nal_end    the end offset of the nal
 @return                 the length of the nal, or 0 if did not find start of nal, or -1 if did not find end of nal
 */
// DEPRECATED - this will be replaced by a similar function with a slightly different API
int find_nal_unit(uint8_t* buf, int size, int* nal_start, int* nal_end)
{
    int i;
    // find start
    *nal_start = 0;
    *nal_end = 0;
    
    i = 0;
    while (   //( next_bits( 24 ) != 0x000001 && next_bits( 32 ) != 0x00000001 )
        (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01) && 
        (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0 || buf[i+3] != 0x01) 
        )
    {
        i++; // skip leading zero
        if (i+4 >= size) { return 0; } // did not find nal start
    }

    if  (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01) // ( next_bits( 24 ) != 0x000001 )
    {
        i++;
    }

    if  (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01) { /* error, should never happen */ return 0; }
    i+= 3;
    *nal_start = i;
    
    while (   //( next_bits( 24 ) != 0x000000 && next_bits( 24 ) != 0x000001 )
        (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0) && 
        (buf[i] != 0 || buf[i+1] != 0 || buf[i+2] != 0x01) 
        )
    {
        i++;
        // FIXME the next line fails when reading a nal that ends exactly at the end of the data
        if (i+3 >= size) { *nal_end = size; return -1; } // did not find nal end, stream ended first
    }
    
    *nal_end = i;
    return (*nal_end - *nal_start);
}


int more_rbsp_data(h264_stream_t* h, bs_t* b) { return !bs_eof(b); }

uint32_t next_bits(bs_t* b, int n) { return 0; } // FIXME UNIMPLEMENTED


/**
 Read a NAL unit from a byte buffer.
 The buffer must start exactly at the beginning of the nal (after the start prefix).
 The NAL is read into h->nal and into other fields within h depending on its type (check h->nal->nal_unit_type after reading).
 @param[in,out] h          the stream object
 @param[in]     buf        the buffer
 @param[in]     size       the size of the buffer
 @return                   the length of data actually read
 */
//7.3.1 NAL unit syntax
int read_nal_unit(h264_stream_t* h, uint8_t* buf, int size)
{
    nal_t* nal = h->nal;

    bs_t* b = (bs_t*)malloc(sizeof(bs_t));;
    bs_init(b, buf, size);

    nal->forbidden_zero_bit = bs_read_f(b,1);
    nal->nal_ref_idc = bs_read_u(b,2);
    nal->nal_unit_type = bs_read_u(b,5);

    uint8_t* rbsp_buf = (uint8_t*)malloc(size);
    int rbsp_size = 0;
    int read_size = 0;
    int i, j;

    // 7.4.1.1 Encapsulation of an SODB within an RBSP

    i = 1; // NOTE omits first byte which contains nal_ref_idc and nal_unit_type, already read
    j = 0;
    while( i < size )
    {
        if( i + 2 < size && 
            buf[i] == 0x00 && buf[i+1] == 0x00 && buf[i+2] == 0x03 ) // next_bits( 24 ) == 0x000003
        {
            rbsp_buf[ j   ] = buf[ i   ];
            rbsp_buf[ j+1 ] = buf[ i+1 ];
            // buf[ i+2 ] == 0x03  // emulation_prevention_three_byte equal to 0x03 // this is guaranteed from the above condition
            i += 3; j += 2;
        }
        else if (i + 2 < size && 
            buf[i] == 0x00 && buf[i+1] == 0x00 && buf[i+2] == 0x01 ) // next_bits( 24 ) == 0x000001 // start of next nal, we're done
        {
            break;
        }
        else
        {
            rbsp_buf[ j ] = buf[ i ];
            i += 1; j += 1;
        }
    }
    read_size = i;
    rbsp_size = j;
    
    // end 7.4.1.1

    bs_init(b, rbsp_buf, rbsp_size); // FIXME DEPRECATED reinit of an already inited bs
    
    if( nal->nal_unit_type == 0) { }                                 //  0    Unspecified
    else if( nal->nal_unit_type == 1) { read_slice_layer_rbsp(h, b); }       //  1    Coded slice of a non-IDR picture
    else if( nal->nal_unit_type == 2) {  }                           //  2    Coded slice data partition A
    else if( nal->nal_unit_type == 3) {  }                           //  3    Coded slice data partition B
    else if( nal->nal_unit_type == 4) {  }                           //  4    Coded slice data partition C
    else if( nal->nal_unit_type == 5) { read_slice_layer_rbsp(h, b); }       //  5    Coded slice of an IDR picture
    else if( nal->nal_unit_type == 6) { /* sei_rbsp( ); */ }         //  6    Supplemental enhancement information (SEI)
    else if( nal->nal_unit_type == 7) { read_seq_parameter_set_rbsp(h, b); } //  7    Sequence parameter set
    else if( nal->nal_unit_type == 8) { read_pic_parameter_set_rbsp(h, b); } //  8    Picture parameter set
    else if( nal->nal_unit_type == 9) { read_access_unit_delimiter_rbsp(h, b); } //  9    Access unit delimiter
    else if( nal->nal_unit_type == 10) { read_end_of_seq_rbsp(h, b); }       // 10    End of sequence       
    else if( nal->nal_unit_type == 11) { read_end_of_stream_rbsp(h, b); }    // 11    End of stream
    else if( nal->nal_unit_type == 12) { /* read_filler_data_rbsp(h, b); */ }      // 12    Filler data
    else if( nal->nal_unit_type == 13) { /* seq_parameter_set_extension_rbsp( ) */ } // 13    Sequence parameter set extension
                                                                     //14..18 Reserved
    else if( nal->nal_unit_type == 19) { read_slice_layer_rbsp(h, b); }      // 19    Coded slice of an auxiliary coded picture without partitioning
                                                                      //20..23 Reserved
                                                                     //24..31 Unspecified

    free(rbsp_buf);
    free(b);

    return read_size;
}


//7.3.2.1 Sequence parameter set RBSP syntax
void read_seq_parameter_set_rbsp(h264_stream_t* h, bs_t* b)
{
    sps_t* sps = h->sps;

    int i;

    sps->profile_idc = bs_read_u8(b);
    sps->constraint_set0_flag = bs_read_u1(b);
    sps->constraint_set1_flag = bs_read_u1(b);
    sps->constraint_set2_flag = bs_read_u1(b);
    sps->constraint_set3_flag = bs_read_u1(b);
    sps->reserved_zero_4bits = bs_read_u(b,4);  /* all 0's */
    sps->level_idc = bs_read_u8(b);
    sps->seq_parameter_set_id = bs_read_ue(b);
    if( sps->profile_idc == 100 || sps->profile_idc == 110 ||
        sps->profile_idc == 122 || sps->profile_idc == 144 )
    {
        sps->chroma_format_idc = bs_read_ue(b);
        if( sps->chroma_format_idc == 3 )
        {
            sps->residual_colour_transform_flag = bs_read_u1(b);
        }
        sps->bit_depth_luma_minus8 = bs_read_ue(b);
        sps->bit_depth_chroma_minus8 = bs_read_ue(b);
        sps->qpprime_y_zero_transform_bypass_flag = bs_read_u1(b);
        sps->seq_scaling_matrix_present_flag = bs_read_u1(b);
        if( sps->seq_scaling_matrix_present_flag )
        {
            for( i = 0; i < 8; i++ )
            {
                sps->seq_scaling_list_present_flag[ i ] = bs_read_u1(b);
                if( sps->seq_scaling_list_present_flag[ i ] )
                {
                    if( i < 6 )
                    {
                        read_scaling_list( b, sps->ScalingList4x4[ i ], 16,
                                      sps->UseDefaultScalingMatrix4x4Flag[ i ]);
                    }
                    else
                    {
                        read_scaling_list( b, sps->ScalingList8x8[ i - 6 ], 64,
                                      sps->UseDefaultScalingMatrix8x8Flag[ i - 6 ] );
                    }
                }
            }
        }
    }
    sps->log2_max_frame_num_minus4 = bs_read_ue(b);
    sps->pic_order_cnt_type = bs_read_ue(b);
    if( sps->pic_order_cnt_type == 0 )
    {
        sps->log2_max_pic_order_cnt_lsb_minus4 = bs_read_ue(b);
    }
    else if( sps->pic_order_cnt_type == 1 )
    {
        sps->delta_pic_order_always_zero_flag = bs_read_u1(b);
        sps->offset_for_non_ref_pic = bs_read_se(b);
        sps->offset_for_top_to_bottom_field = bs_read_se(b);
        sps->num_ref_frames_in_pic_order_cnt_cycle = bs_read_ue(b);
        for( i = 0; i < sps->num_ref_frames_in_pic_order_cnt_cycle; i++ )
        {
            sps->offset_for_ref_frame[ i ] = bs_read_se(b);
        }
    }
    sps->num_ref_frames = bs_read_ue(b);
    sps->gaps_in_frame_num_value_allowed_flag = bs_read_u1(b);
    sps->pic_width_in_mbs_minus1 = bs_read_ue(b);
    sps->pic_height_in_map_units_minus1 = bs_read_ue(b);
    sps->frame_mbs_only_flag = bs_read_u1(b);
    if( !sps->frame_mbs_only_flag )
    {
        sps->mb_adaptive_frame_field_flag = bs_read_u1(b);
    }
    sps->direct_8x8_inference_flag = bs_read_u1(b);
    sps->frame_cropping_flag = bs_read_u1(b);
    if( sps->frame_cropping_flag )
    {
        sps->frame_crop_left_offset = bs_read_ue(b);
        sps->frame_crop_right_offset = bs_read_ue(b);
        sps->frame_crop_top_offset = bs_read_ue(b);
        sps->frame_crop_bottom_offset = bs_read_ue(b);
    }
    sps->vui_parameters_present_flag = bs_read_u1(b);
    if( sps->vui_parameters_present_flag )
    {
        read_vui_parameters(h, b);
    }
    read_rbsp_trailing_bits(h, b);
}


//7.3.2.1.1 Scaling list syntax
void read_scaling_list(bs_t* b, int* scalingList, int sizeOfScalingList, int useDefaultScalingMatrixFlag )
{
    int j;

    int lastScale = 8;
    int nextScale = 8;
    for( j = 0; j < sizeOfScalingList; j++ )
    {
        if( nextScale != 0 )
        {
            int delta_scale = bs_read_se(b);
            nextScale = ( lastScale + delta_scale + 256 ) % 256;
            useDefaultScalingMatrixFlag = ( j == 0 && nextScale == 0 );
        }
        scalingList[ j ] = ( nextScale == 0 ) ? lastScale : nextScale;
        lastScale = scalingList[ j ];
    }
}

//Appendix E.1.1 VUI parameters syntax
void read_vui_parameters(h264_stream_t* h, bs_t* b)
{
    sps_t* sps = h->sps;

    sps->vui.aspect_ratio_info_present_flag = bs_read_u1(b);
    if( sps->vui.aspect_ratio_info_present_flag )
    {
        sps->vui.aspect_ratio_idc = bs_read_u8(b);
        if( sps->vui.aspect_ratio_idc == SAR_Extended )
        {
            sps->vui.sar_width = bs_read_u(b,16);
            sps->vui.sar_height = bs_read_u(b,16);
        }
    }
    sps->vui.overscan_info_present_flag = bs_read_u1(b);
    if( sps->vui.overscan_info_present_flag )
    {
        sps->vui.overscan_appropriate_flag = bs_read_u1(b);
    }
    sps->vui.video_signal_type_present_flag = bs_read_u1(b);
    if( sps->vui.video_signal_type_present_flag )
    {
        sps->vui.video_format = bs_read_u(b,3);
        sps->vui.video_full_range_flag = bs_read_u1(b);
        sps->vui.colour_description_present_flag = bs_read_u1(b);
        if( sps->vui.colour_description_present_flag )
        {
            sps->vui.colour_primaries = bs_read_u8(b);
            sps->vui.transfer_characteristics = bs_read_u8(b);
            sps->vui.matrix_coefficients = bs_read_u8(b);
        }
    }
    sps->vui.chroma_loc_info_present_flag = bs_read_u1(b);
    if( sps->vui.chroma_loc_info_present_flag )
    {
        sps->vui.chroma_sample_loc_type_top_field = bs_read_ue(b);
        sps->vui.chroma_sample_loc_type_bottom_field = bs_read_ue(b);
    }
    sps->vui.timing_info_present_flag = bs_read_u1(b);
    if( sps->vui.timing_info_present_flag )
    {
        sps->vui.num_units_in_tick = bs_read_u(b,32);
        sps->vui.time_scale = bs_read_u(b,32);
        sps->vui.fixed_frame_rate_flag = bs_read_u1(b);
    }
    sps->vui.nal_hrd_parameters_present_flag = bs_read_u1(b);
    if( sps->vui.nal_hrd_parameters_present_flag )
    {
        read_hrd_parameters(h, b);
    }
    sps->vui.vcl_hrd_parameters_present_flag = bs_read_u1(b);
    if( sps->vui.vcl_hrd_parameters_present_flag )
    {
        read_hrd_parameters(h, b);
    }
    if( sps->vui.nal_hrd_parameters_present_flag || sps->vui.vcl_hrd_parameters_present_flag )
    {
        sps->vui.low_delay_hrd_flag = bs_read_u1(b);
    }
    sps->vui.pic_struct_present_flag = bs_read_u1(b);
    sps->vui.bitstream_restriction_flag = bs_read_u1(b);
    if( sps->vui.bitstream_restriction_flag )
    {
        sps->vui.motion_vectors_over_pic_boundaries_flag = bs_read_u1(b);
        sps->vui.max_bytes_per_pic_denom = bs_read_ue(b);
        sps->vui.max_bits_per_mb_denom = bs_read_ue(b);
        sps->vui.log2_max_mv_length_horizontal = bs_read_ue(b);
        sps->vui.log2_max_mv_length_vertical = bs_read_ue(b);
        sps->vui.num_reorder_frames = bs_read_ue(b);
        sps->vui.max_dec_frame_buffering = bs_read_ue(b);
    }
}


//Appendix E.1.2 HRD parameters syntax
void read_hrd_parameters(h264_stream_t* h, bs_t* b)
{
    sps_t* sps = h->sps;
    int SchedSelIdx;

    sps->hrd.cpb_cnt_minus1 = bs_read_ue(b);
    sps->hrd.bit_rate_scale = bs_read_u(b,4);
    sps->hrd.cpb_size_scale = bs_read_u(b,4);
    for( SchedSelIdx = 0; SchedSelIdx <= sps->hrd.cpb_cnt_minus1; SchedSelIdx++ )
    {
        sps->hrd.bit_rate_value_minus1[ SchedSelIdx ] = bs_read_ue(b);
        sps->hrd.cpb_size_value_minus1[ SchedSelIdx ] = bs_read_ue(b);
        sps->hrd.cbr_flag[ SchedSelIdx ] = bs_read_u1(b);
    }
    sps->hrd.initial_cpb_removal_delay_length_minus1 = bs_read_u(b,5);
    sps->hrd.cpb_removal_delay_length_minus1 = bs_read_u(b,5);
    sps->hrd.dpb_output_delay_length_minus1 = bs_read_u(b,5);
    sps->hrd.time_offset_length = bs_read_u(b,5);
}


/*
UNIMPLEMENTED
//7.3.2.1.2 Sequence parameter set extension RBSP syntax
int read_seq_parameter_set_extension_rbsp(bs_t* b, sps_ext_t* sps_ext) {
    seq_parameter_set_id = bs_read_ue(b);
    aux_format_idc = bs_read_ue(b);
    if( aux_format_idc != 0 ) {
        bit_depth_aux_minus8 = bs_read_ue(b);
        alpha_incr_flag = bs_read_u1(b);
        alpha_opaque_value = bs_read_u(v);
        alpha_transparent_value = bs_read_u(v);
    }
    additional_extension_flag = bs_read_u1(b);
    read_rbsp_trailing_bits();
}
*/

//7.3.2.2 Picture parameter set RBSP syntax
void read_pic_parameter_set_rbsp(h264_stream_t* h, bs_t* b)
{
    pps_t* pps = h->pps;

    int i;
    int i_group;

    pps->pic_parameter_set_id = bs_read_ue(b);
    pps->seq_parameter_set_id = bs_read_ue(b);
    pps->entropy_coding_mode_flag = bs_read_u1(b);
    pps->pic_order_present_flag = bs_read_u1(b);
    pps->num_slice_groups_minus1 = bs_read_ue(b);
    if( pps->num_slice_groups_minus1 > 0 )
    {
        pps->slice_group_map_type = bs_read_ue(b);
        if( pps->slice_group_map_type == 0 )
        {
            for( i_group = 0; i_group <= pps->num_slice_groups_minus1; i_group++ )
            {
                pps->run_length_minus1[ i_group ] = bs_read_ue(b);
            }
        }
        else if( pps->slice_group_map_type == 2 )
        {
            for( i_group = 0; i_group < pps->num_slice_groups_minus1; i_group++ )
            {
                pps->top_left[ i_group ] = bs_read_ue(b);
                pps->bottom_right[ i_group ] = bs_read_ue(b);
            }
        }
        else if( pps->slice_group_map_type == 3 ||
                 pps->slice_group_map_type == 4 ||
                 pps->slice_group_map_type == 5 )
        {
            pps->slice_group_change_direction_flag = bs_read_u1(b);
            pps->slice_group_change_rate_minus1 = bs_read_ue(b);
        }
        else if( pps->slice_group_map_type == 6 )
        {
            pps->pic_size_in_map_units_minus1 = bs_read_ue(b);
            for( i = 0; i <= pps->pic_size_in_map_units_minus1; i++ )
            {
                pps->slice_group_id[ i ] = bs_read_u(b, ceil( log2( pps->num_slice_groups_minus1 + 1 ) ) ); // was u(v)
            }
        }
    }
    pps->num_ref_idx_l0_active_minus1 = bs_read_ue(b);
    pps->num_ref_idx_l1_active_minus1 = bs_read_ue(b);
    pps->weighted_pred_flag = bs_read_u1(b);
    pps->weighted_bipred_idc = bs_read_u(b,2);
    pps->pic_init_qp_minus26 = bs_read_se(b);
    pps->pic_init_qs_minus26 = bs_read_se(b);
    pps->chroma_qp_index_offset = bs_read_se(b);
    pps->deblocking_filter_control_present_flag = bs_read_u1(b);
    pps->constrained_intra_pred_flag = bs_read_u1(b);
    pps->redundant_pic_cnt_present_flag = bs_read_u1(b);
    if( more_rbsp_data(h, b) )
    {
        pps->transform_8x8_mode_flag = bs_read_u1(b);
        pps->pic_scaling_matrix_present_flag = bs_read_u1(b);
        if( pps->pic_scaling_matrix_present_flag )
        {
            for( i = 0; i < 6 + 2* pps->transform_8x8_mode_flag; i++ )
            {
                pps->pic_scaling_list_present_flag[ i ] = bs_read_u1(b);
                if( pps->pic_scaling_list_present_flag[ i ] )
                {
                    if( i < 6 )
                    {
                        read_scaling_list( b, pps->ScalingList4x4[ i ], 16,
                                      pps->UseDefaultScalingMatrix4x4Flag[ i ] );
                    }
                    else
                    {
                        read_scaling_list( b, pps->ScalingList8x8[ i - 6 ], 64,
                                      pps->UseDefaultScalingMatrix8x8Flag[ i - 6 ] );
                    }
                }
            }
        }
        pps->second_chroma_qp_index_offset = bs_read_se(b);
    }
    read_rbsp_trailing_bits(h, b);
}

/*
// UNIMPLEMENTED
//7.3.2.3 Supplemental enhancement information RBSP syntax
read_sei_rbsp( ) {
    do {
        read_sei_message( );
    } while( more_rbsp_data( ) );
    read_rbsp_trailing_bits( );
}

//7.3.2.3.1 Supplemental enhancement information message syntax
read_sei_message( ) {
    payloadType = 0;
    while( next_bits( 8 ) == 0xFF ) {
        ff_byte = bs_read_f(8); // equal to 0xFF
        payloadType += 255;
    }
    last_payload_type_byte = bs_read_u8(b);
    payloadType += last_payload_type_byte;
    payloadSize = 0;
    while( next_bits( 8 ) == 0xFF ) {
        ff_byte = bs_read_f(8); // equal to 0xFF
        payloadSize += 255;
    }
    last_payload_size_byte = bs_read_u8(b);
    payloadSize += last_payload_size_byte;
    sei_payload( payloadType, payloadSize );
}
*/

//7.3.2.4 Access unit delimiter RBSP syntax
void read_access_unit_delimiter_rbsp(h264_stream_t* h, bs_t* b)
{
    int primary_pic_type = bs_read_u(b,3); //FIXME
    read_rbsp_trailing_bits(h, b);
}

//7.3.2.5 End of sequence RBSP syntax
void read_end_of_seq_rbsp(h264_stream_t* h, bs_t* b)
{
}

//7.3.2.6 End of stream RBSP syntax
void read_end_of_stream_rbsp(h264_stream_t* h, bs_t* b)
{
}

//7.3.2.7 Filler data RBSP syntax
void read_filler_data_rbsp(h264_stream_t* h, bs_t* b)
{
    int ff_byte; //FIXME
    while( next_bits(b, 8) == 0xFF )
    {
        ff_byte = bs_read_f(b,8);  // equal to 0xFF
    }
    read_rbsp_trailing_bits(h, b);
}

//7.3.2.8 Slice layer without partitioning RBSP syntax
void read_slice_layer_rbsp(h264_stream_t* h, bs_t* b)
{
    read_slice_header(h, b);

    // DEBUG
    //printf("slice data: \n");
    //debug_bytes(b->p, b->end - b->p);
    //printf("bits left in front: %d \n", b->bits_left);

    // FIXME should read or skip data
    //slice_data( ); /* all categories of slice_data( ) syntax */  
    //read_rbsp_slice_trailing_bits(h, b);
}

/*
// UNIMPLEMENTED
//7.3.2.9.1 Slice data partition A RBSP syntax
slice_data_partition_a_layer_rbsp( ) {
    read_slice_header( );             // only category 2
    slice_id = bs_read_ue(b)
    read_slice_data( );               // only category 2
    read_rbsp_slice_trailing_bits( ); // only category 2
}

//7.3.2.9.2 Slice data partition B RBSP syntax
slice_data_partition_b_layer_rbsp( ) {
    slice_id = bs_read_ue(b);    // only category 3
    if( redundant_pic_cnt_present_flag )
        redundant_pic_cnt = bs_read_ue(b);
    read_slice_data( );               // only category 3
    read_rbsp_slice_trailing_bits( ); // only category 3
}

//7.3.2.9.3 Slice data partition C RBSP syntax
slice_data_partition_c_layer_rbsp( ) {
    slice_id = bs_read_ue(b);    // only category 4
    if( redundant_pic_cnt_present_flag )
        redundant_pic_cnt = bs_read_ue(b);
    read_slice_data( );               // only category 4
    rbsp_slice_trailing_bits( ); // only category 4
}
*/

int
more_rbsp_trailing_data(h264_stream_t* h, bs_t* b) { return !bs_eof(b); }

//7.3.2.10 RBSP slice trailing bits syntax
void read_rbsp_slice_trailing_bits(h264_stream_t* h, bs_t* b)
{
    read_rbsp_trailing_bits(h, b);
    int cabac_zero_word;
    if( h->pps->entropy_coding_mode_flag )
    {
        while( more_rbsp_trailing_data(h, b) )
        {
            cabac_zero_word = bs_read_f(b,16); // equal to 0x0000
        }
    }
}

//7.3.2.11 RBSP trailing bits syntax
void read_rbsp_trailing_bits(h264_stream_t* h, bs_t* b)
{
    int rbsp_stop_one_bit;
    int rbsp_alignment_zero_bit;
    if( !bs_byte_aligned(b) )
    {
        rbsp_stop_one_bit = bs_read_f(b,1); // equal to 1
        while( !bs_byte_aligned(b) )
        {
            rbsp_alignment_zero_bit = bs_read_f(b,1); // equal to 0
        }
    }
}

//7.3.3 Slice header syntax
void read_slice_header(h264_stream_t* h, bs_t* b)
{
    slice_header_t* sh = h->sh;
    sps_t* sps = h->sps;
    pps_t* pps = h->pps;
    nal_t* nal = h->nal;

    sh->first_mb_in_slice = bs_read_ue(b);
    sh->slice_type = bs_read_ue(b);
    if (sh->slice_type > 4) { sh->slice_type -= 5; } // FIXME this changes slicetypes to (supposedly) equivalent ones
    sh->pic_parameter_set_id = bs_read_ue(b);
    sh->frame_num = bs_read_u(b, sps->log2_max_frame_num_minus4 + 4 ); // was u(v)
    if( !sps->frame_mbs_only_flag )
    {
        sh->field_pic_flag = bs_read_u1(b);
        if( sh->field_pic_flag )
        {
            sh->bottom_field_flag = bs_read_u1(b);
        }
    }
    if( nal->nal_unit_type == 5 )
    {
        sh->idr_pic_id = bs_read_ue(b);
    }
    if( sps->pic_order_cnt_type == 0 )
    {
        sh->pic_order_cnt_lsb = bs_read_u(b, sps->log2_max_pic_order_cnt_lsb_minus4 + 4 ); // was u(v)
        if( pps->pic_order_present_flag && !sh->field_pic_flag )
        {
            sh->delta_pic_order_cnt_bottom = bs_read_se(b);
        }
    }
    if( sps->pic_order_cnt_type == 1 && !sps->delta_pic_order_always_zero_flag )
    {
        sh->delta_pic_order_cnt[ 0 ] = bs_read_se(b);
        if( pps->pic_order_present_flag && !sh->field_pic_flag )
        {
            sh->delta_pic_order_cnt[ 1 ] = bs_read_se(b);
        }
    }
    if( pps->redundant_pic_cnt_present_flag )
    {
        sh->redundant_pic_cnt = bs_read_ue(b);
    }
    if( sh->slice_type == SH_SLICE_TYPE_B )
    {
        sh->direct_spatial_mv_pred_flag = bs_read_u1(b);
    }
    if( sh->slice_type == SH_SLICE_TYPE_P || sh->slice_type == SH_SLICE_TYPE_SP || sh->slice_type == SH_SLICE_TYPE_B )
    {
        sh->num_ref_idx_active_override_flag = bs_read_u1(b);
        if( sh->num_ref_idx_active_override_flag )
        {
            sh->num_ref_idx_l0_active_minus1 = bs_read_ue(b); // FIXME does this modify the pps?
            if( sh->slice_type == SH_SLICE_TYPE_B )
            {
                sh->num_ref_idx_l1_active_minus1 = bs_read_ue(b);
            }
        }
    }
    read_ref_pic_list_reordering(h, b);
    if( ( pps->weighted_pred_flag && ( sh->slice_type == SH_SLICE_TYPE_P || sh->slice_type == SH_SLICE_TYPE_SP ) ) ||
        ( pps->weighted_bipred_idc == 1 && sh->slice_type == SH_SLICE_TYPE_B ) )
    {
        read_pred_weight_table(h, b);
    }
    if( nal->nal_ref_idc != 0 )
    {
        read_dec_ref_pic_marking(h, b);
    }
    if( pps->entropy_coding_mode_flag && sh->slice_type != SH_SLICE_TYPE_I && sh->slice_type != SH_SLICE_TYPE_SI )
    {
        sh->cabac_init_idc = bs_read_ue(b);
    }
    sh->slice_qp_delta = bs_read_se(b);
    if( sh->slice_type == SH_SLICE_TYPE_SP || sh->slice_type == SH_SLICE_TYPE_SI )
    {
        if( sh->slice_type == SH_SLICE_TYPE_SP )
        {
            sh->sp_for_switch_flag = bs_read_u1(b);
        }
        sh->slice_qs_delta = bs_read_se(b);
    }
    if( pps->deblocking_filter_control_present_flag )
    {
        sh->disable_deblocking_filter_idc = bs_read_ue(b);
        if( sh->disable_deblocking_filter_idc != 1 )
        {
            sh->slice_alpha_c0_offset_div2 = bs_read_se(b);
            sh->slice_beta_offset_div2 = bs_read_se(b);
        }
    }
    if( pps->num_slice_groups_minus1 > 0 &&
        pps->slice_group_map_type >= 3 && pps->slice_group_map_type <= 5)
    {
        sh->slice_group_change_cycle = 
            bs_read_u(b, ceil( log2( pps->pic_size_in_map_units_minus1 +  
                                     pps->slice_group_change_rate_minus1 + 1 ) ) ); // was u(v) // FIXME add 2?
    }
}

//7.3.3.1 Reference picture list reordering syntax
void read_ref_pic_list_reordering(h264_stream_t* h, bs_t* b)
{
    slice_header_t* sh = h->sh;

    if( sh->slice_type != SH_SLICE_TYPE_I && sh->slice_type != SH_SLICE_TYPE_SI )
    {
        sh->rplr.ref_pic_list_reordering_flag_l0 = bs_read_u1(b);
        if( sh->rplr.ref_pic_list_reordering_flag_l0 )
        {
            do
            {
                sh->rplr.reordering_of_pic_nums_idc = bs_read_ue(b);
                if( sh->rplr.reordering_of_pic_nums_idc == 0 ||
                    sh->rplr.reordering_of_pic_nums_idc == 1 )
                {
                    sh->rplr.abs_diff_pic_num_minus1 = bs_read_ue(b);
                }
                else if( sh->rplr.reordering_of_pic_nums_idc == 2 )
                {
                    sh->rplr.long_term_pic_num = bs_read_ue(b);
                }
            } while( sh->rplr.reordering_of_pic_nums_idc != 3 && ! bs_eof(b) );
        }
    }
    if( sh->slice_type == SH_SLICE_TYPE_B )
    {
        sh->rplr.ref_pic_list_reordering_flag_l1 = bs_read_u1(b);
        if( sh->rplr.ref_pic_list_reordering_flag_l1 )
        {
            do 
            {
                sh->rplr.reordering_of_pic_nums_idc = bs_read_ue(b);
                if( sh->rplr.reordering_of_pic_nums_idc == 0 ||
                    sh->rplr.reordering_of_pic_nums_idc == 1 )
                {
                    sh->rplr.abs_diff_pic_num_minus1 = bs_read_ue(b);
                }
                else if( sh->rplr.reordering_of_pic_nums_idc == 2 )
                {
                    sh->rplr.long_term_pic_num = bs_read_ue(b);
                }
            } while( sh->rplr.reordering_of_pic_nums_idc != 3 && ! bs_eof(b) );
        }
    }
}

//7.3.3.2 Prediction weight table syntax
void read_pred_weight_table(h264_stream_t* h, bs_t* b)
{
    slice_header_t* sh = h->sh;
    sps_t* sps = h->sps;
    pps_t* pps = h->pps;

    int i, j;

    sh->pwt.luma_log2_weight_denom = bs_read_ue(b);
    if( sps->chroma_format_idc != 0 )
    {
        sh->pwt.chroma_log2_weight_denom = bs_read_ue(b);
    }
    for( i = 0; i <= pps->num_ref_idx_l0_active_minus1; i++ )
    {
        sh->pwt.luma_weight_l0_flag = bs_read_u1(b);
        if( sh->pwt.luma_weight_l0_flag )
        {
            sh->pwt.luma_weight_l0[ i ] = bs_read_se(b);
            sh->pwt.luma_offset_l0[ i ] = bs_read_se(b);
        }
        if ( sps->chroma_format_idc != 0 )
        {
            sh->pwt.chroma_weight_l0_flag = bs_read_u1(b);
            if( sh->pwt.chroma_weight_l0_flag )
            {
                for( j =0; j < 2; j++ )
                {
                    sh->pwt.chroma_weight_l0[ i ][ j ] = bs_read_se(b);
                    sh->pwt.chroma_offset_l0[ i ][ j ] = bs_read_se(b);
                }
            }
        }
    }
    if( sh->slice_type == SH_SLICE_TYPE_B )
    {
        for( i = 0; i <= pps->num_ref_idx_l1_active_minus1; i++ )
        {
            sh->pwt.luma_weight_l1_flag = bs_read_u1(b);
            if( sh->pwt.luma_weight_l1_flag )
            {
                sh->pwt.luma_weight_l1[ i ] = bs_read_se(b);
                sh->pwt.luma_offset_l1[ i ] = bs_read_se(b);
            }
            if( sps->chroma_format_idc != 0 )
            {
                sh->pwt.chroma_weight_l1_flag = bs_read_u1(b);
                if( sh->pwt.chroma_weight_l1_flag )
                {
                    for( j = 0; j < 2; j++ )
                    {
                        sh->pwt.chroma_weight_l1[ i ][ j ] = bs_read_se(b);
                        sh->pwt.chroma_offset_l1[ i ][ j ] = bs_read_se(b);
                    }
                }
            }
        }
    }
}

//7.3.3.3 Decoded reference picture marking syntax
void read_dec_ref_pic_marking(h264_stream_t* h, bs_t* b)
{
    slice_header_t* sh = h->sh;

    if( h->nal->nal_unit_type == 5 )
    {
        sh->drpm.no_output_of_prior_pics_flag = bs_read_u1(b);
        sh->drpm.long_term_reference_flag = bs_read_u1(b);
    }
    else
    {
        sh->drpm.adaptive_ref_pic_marking_mode_flag = bs_read_u1(b);
        if( sh->drpm.adaptive_ref_pic_marking_mode_flag )
        {
            do
            {
                sh->drpm.memory_management_control_operation = bs_read_ue(b);
                if( sh->drpm.memory_management_control_operation == 1 ||
                    sh->drpm.memory_management_control_operation == 3 )
                {
                    sh->drpm.difference_of_pic_nums_minus1 = bs_read_ue(b);
                }
                if(sh->drpm.memory_management_control_operation == 2 )
                {
                    sh->drpm.long_term_pic_num = bs_read_ue(b);
                }
                if( sh->drpm.memory_management_control_operation == 3 ||
                    sh->drpm.memory_management_control_operation == 6 )
                {
                    sh->drpm.long_term_frame_idx = bs_read_ue(b);
                }
                if( sh->drpm.memory_management_control_operation == 4 )
                {
                    sh->drpm.max_long_term_frame_idx_plus1 = bs_read_ue(b);
                }
            } while( sh->drpm.memory_management_control_operation != 0 && ! bs_eof(b) );
        }
    }
}

/*
// UNIMPLEMENTED
//7.3.4 Slice data syntax
slice_data( )
{
    if( pps->entropy_coding_mode_flag )
        while( !byte_aligned( ) )
            cabac_alignment_one_bit = bs_read_f(1);
    CurrMbAddr = first_mb_in_slice * ( 1 + MbaffFrameFlag );
    moreDataFlag = 1;
    prevMbSkipped = 0;
    do {
        if( sh->slice_type != SH_SLICE_TYPE_I && sh->slice_type != SH_SLICE_TYPE_SI )
            if( !pps->entropy_coding_mode_flag ) {
                mb_skip_run = bs_read_ue(b);
                prevMbSkipped = ( mb_skip_run > 0 );
                for( i=0; i<mb_skip_run; i++ )
                    CurrMbAddr = NextMbAddress( CurrMbAddr );
                moreDataFlag = more_rbsp_data( );
            } else {
                mb_skip_flag = bs_read_ae(v);
                moreDataFlag = !mb_skip_flag;
            }
        if( moreDataFlag ) {
            if( MbaffFrameFlag && ( CurrMbAddr % 2 == 0 ||
                                    ( CurrMbAddr % 2 == 1 && prevMbSkipped ) ) )
                mb_field_decoding_flag = bs_read_u1(b) | bs_read_ae(v);
            macroblock_layer( );
        }
        if( !pps->entropy_coding_mode_flag )
            moreDataFlag = more_rbsp_data( );
        else {
            if( sh->slice_type != SH_SLICE_TYPE_I && sh->slice_type != SH_SLICE_TYPE_SI )
                prevMbSkipped = mb_skip_flag;
            if( MbaffFrameFlag && CurrMbAddr % 2 == 0 )
                moreDataFlag = 1;
            else {
                end_of_slice_flag = bs_read_ae(v);
                moreDataFlag = !end_of_slice_flag;
            }
        }
        CurrMbAddr = NextMbAddress( CurrMbAddr );
    } while( moreDataFlag );
}
*/


/****** writing ******/

#define DBG_START \
    bs_t* b2 = (bs_t*)malloc(sizeof(bs_t)); \
    bs_init(b2, b->p, b->end - b->p); \
    h264_stream_t* h2 = (h264_stream_t*)malloc(sizeof(h264_stream_t));\
    h2->sps=h->sps; h2->pps=h->pps; h2->nal=h->nal; h2->sh=h->sh;  \

#define DBG_END \
    free(h2); \
    free(b2); \

#define DBG \
  printf("line %d:",  __LINE__ ); \
  debug_bs(b); \
  b2->p = b2->start; b2->bits_left = 8; \
  /* read_slice_header(h2, b2); */\
  /* if (h2->sh->drpm.adaptive_ref_pic_marking_mode_flag) { printf(" X"); }; */ \
  printf("\n"); \

/**
 Write a NAL unit to a byte buffer.
 The NAL which is written out has a type determined by h->nal and data which comes from other fields within h depending on its type.
 @param[in,out]  h          the stream object
 @param[out]     buf        the buffer
 @param[in]      size       the size of the buffer
 @return                    the length of data actually written
 */
//7.3.1 NAL unit syntax
int write_nal_unit(h264_stream_t* h, uint8_t* buf, int size)
{
    nal_t* nal = h->nal;

    bs_t* b = (bs_t*)malloc(sizeof(bs_t));;
    bs_init(b, buf, size);

    bs_write_f(b,1, nal->forbidden_zero_bit);
    bs_write_u(b,2, nal->nal_ref_idc);
    bs_write_u(b,5, nal->nal_unit_type);

    uint8_t* rbsp_buf = (uint8_t*)malloc(size); // NOTE this may have to be slightly smaller (4/3 smaller, worst case) in order to be guaranteed to fit
    int rbsp_size = 0;
    int write_size = 0;
    int i, j;

    bs_init(b, rbsp_buf, size); // FIXME DEPRECATED reinit of an already inited bs
    
    if( nal->nal_unit_type == 0) { }                                 //  0    Unspecified
    else if( nal->nal_unit_type == 1) { write_slice_layer_rbsp(h, b); }       //  1    Coded slice of a non-IDR picture
    else if( nal->nal_unit_type == 2) {  }                           //  2    Coded slice data partition A
    else if( nal->nal_unit_type == 3) {  }                           //  3    Coded slice data partition B
    else if( nal->nal_unit_type == 4) {  }                           //  4    Coded slice data partition C
    else if( nal->nal_unit_type == 5) { write_slice_layer_rbsp(h, b); }       //  5    Coded slice of an IDR picture
    else if( nal->nal_unit_type == 6) { /* sei_rbsp( ); */ }         //  6    Supplemental enhancement information (SEI)
    else if( nal->nal_unit_type == 7) { write_seq_parameter_set_rbsp(h, b); } //  7    Sequence parameter set
    else if( nal->nal_unit_type == 8) { write_pic_parameter_set_rbsp(h, b); } //  8    Picture parameter set
    else if( nal->nal_unit_type == 9) { write_access_unit_delimiter_rbsp(h, b); } //  9    Access unit delimiter
    else if( nal->nal_unit_type == 10) { write_end_of_seq_rbsp(h, b); }       // 10    End of sequence       
    else if( nal->nal_unit_type == 11) { write_end_of_stream_rbsp(h, b); }    // 11    End of stream
    else if( nal->nal_unit_type == 12) { /* write_filler_data_rbsp(h, b); */ }      // 12    Filler data
    else if( nal->nal_unit_type == 13) { /* seq_parameter_set_extension_rbsp( ) */ } // 13    Sequence parameter set extension
                                                                     //14..18 Reserved
    else if( nal->nal_unit_type == 19) { write_slice_layer_rbsp(h, b); }      // 19    Coded slice of an auxiliary coded picture without partitioning
                                                                      //20..23 Reserved
                                                                     //24..31 Unspecified

    rbsp_size = bs_pos(b);

    // 7.4.1.1 Encapsulation of an SODB within an RBSP

    i = 1; // NOTE omits first byte which contains nal_ref_idc and nal_unit_type, already written
    j = 0;
    while( i < size && j < rbsp_size )
    {
        if( i + 3 < size && j + 2 < rbsp_size &&
            rbsp_buf[j] == 0x00 && rbsp_buf[j+1] == 0x00 && 
            ( rbsp_buf[j+2] == 0x01 || rbsp_buf[j+2] == 0x02 || rbsp_buf[j+2] == 0x03 ) ) // next_bits( 24 ) == 0x000001 or 0x000002 or 0x000003
        {
            buf[ i   ] = rbsp_buf[ j   ];
            buf[ i+1 ] = rbsp_buf[ j+1 ];
            buf[ i+2 ] = 0x03;  // emulation_prevention_three_byte equal to 0x03
            buf[ i+3 ] = rbsp_buf[ j+2 ];
            i += 4; j += 3;
        }
        else if ( j == rbsp_size - 1 && 
                  rbsp_buf[ j ] == 0x00 )
        {
            buf[ i ] == 0x03; // emulation_prevention_three_byte equal to 0x03 in trailing position
            i += 1;
        }
        else
        {
            buf[ i ] = rbsp_buf[ j ];
            i += 1; j+= 1;
        }
    }
    write_size = i;

    // end 7.4.1.1

    free(rbsp_buf);
    free(b);

    return write_size;
}


//7.3.2.1 Sequence parameter set RBSP syntax
void write_seq_parameter_set_rbsp(h264_stream_t* h, bs_t* b)
{
    sps_t* sps = h->sps;

    int i;

    bs_write_u8(b, sps->profile_idc);
    bs_write_u1(b, sps->constraint_set0_flag);
    bs_write_u1(b, sps->constraint_set1_flag);
    bs_write_u1(b, sps->constraint_set2_flag);
    bs_write_u1(b, sps->constraint_set3_flag);
    bs_write_u(b,4, sps->reserved_zero_4bits);  /* all 0's */
    bs_write_u8(b, sps->level_idc);
    bs_write_ue(b, sps->seq_parameter_set_id);
    if( sps->profile_idc == 100 || sps->profile_idc == 110 ||
        sps->profile_idc == 122 || sps->profile_idc == 144 )
    {
        bs_write_ue(b, sps->chroma_format_idc);
        if( sps->chroma_format_idc == 3 )
        {
            bs_write_u1(b, sps->residual_colour_transform_flag);
        }
        bs_write_ue(b, sps->bit_depth_luma_minus8);
        bs_write_ue(b, sps->bit_depth_chroma_minus8);
        bs_write_u1(b, sps->qpprime_y_zero_transform_bypass_flag);
        bs_write_u1(b, sps->seq_scaling_matrix_present_flag);
        if( sps->seq_scaling_matrix_present_flag )
        {
            for( i = 0; i < 8; i++ )
            {
                bs_write_u1(b, sps->seq_scaling_list_present_flag[ i ]);
                if( sps->seq_scaling_list_present_flag[ i ] )
                {
                    if( i < 6 )
                    {
                        write_scaling_list( b, sps->ScalingList4x4[ i ], 16,
                                      sps->UseDefaultScalingMatrix4x4Flag[ i ]);
                    }
                    else
                    {
                        write_scaling_list( b, sps->ScalingList8x8[ i - 6 ], 64,
                                      sps->UseDefaultScalingMatrix8x8Flag[ i - 6 ] );
                    }
                }
            }
        }
    }
    bs_write_ue(b, sps->log2_max_frame_num_minus4);
    bs_write_ue(b, sps->pic_order_cnt_type);
    if( sps->pic_order_cnt_type == 0 )
    {
        bs_write_ue(b, sps->log2_max_pic_order_cnt_lsb_minus4);
    }
    else if( sps->pic_order_cnt_type == 1 )
    {
        bs_write_u1(b, sps->delta_pic_order_always_zero_flag);
        bs_write_se(b, sps->offset_for_non_ref_pic);
        bs_write_se(b, sps->offset_for_top_to_bottom_field);
        bs_write_ue(b, sps->num_ref_frames_in_pic_order_cnt_cycle);
        for( i = 0; i < sps->num_ref_frames_in_pic_order_cnt_cycle; i++ )
        {
            bs_write_se(b, sps->offset_for_ref_frame[ i ]);
        }
    }
    bs_write_ue(b, sps->num_ref_frames);
    bs_write_u1(b, sps->gaps_in_frame_num_value_allowed_flag);
    bs_write_ue(b, sps->pic_width_in_mbs_minus1);
    bs_write_ue(b, sps->pic_height_in_map_units_minus1);
    bs_write_u1(b, sps->frame_mbs_only_flag);
    if( !sps->frame_mbs_only_flag )
    {
        bs_write_u1(b, sps->mb_adaptive_frame_field_flag);
    }
    bs_write_u1(b, sps->direct_8x8_inference_flag);
    bs_write_u1(b, sps->frame_cropping_flag);
    if( sps->frame_cropping_flag )
    {
        bs_write_ue(b, sps->frame_crop_left_offset);
        bs_write_ue(b, sps->frame_crop_right_offset);
        bs_write_ue(b, sps->frame_crop_top_offset);
        bs_write_ue(b, sps->frame_crop_bottom_offset);
    }
    bs_write_u1(b, sps->vui_parameters_present_flag);
    if( sps->vui_parameters_present_flag )
    {
        write_vui_parameters(h, b);
    }
    write_rbsp_trailing_bits(h, b);
}

//7.3.2.1.1 Scaling list syntax
void write_scaling_list(bs_t* b, int* scalingList, int sizeOfScalingList, int useDefaultScalingMatrixFlag )
{
    int j;

    int lastScale = 8;
    int nextScale = 8;
    for( j = 0; j < sizeOfScalingList; j++ )
    {
        int delta_scale;
        if( nextScale != 0 )
        {
            // FIXME
            /*
            nextScale = ( lastScale + delta_scale + 256 ) % 256;
            useDefaultScalingMatrixFlag = ( j == 0 && nextScale == 0 );
            */
            bs_write_se(b, delta_scale);
        }
        scalingList[ j ] = ( nextScale == 0 ) ? lastScale : nextScale;
        lastScale = scalingList[ j ];
    }
}

//Appendix E.1.1 VUI parameters syntax
void
write_vui_parameters(h264_stream_t* h, bs_t* b)
{
    sps_t* sps = h->sps;

    bs_write_u1(b, sps->vui.aspect_ratio_info_present_flag);
    if( sps->vui.aspect_ratio_info_present_flag )
    {
        bs_write_u8(b, sps->vui.aspect_ratio_idc);
        if( sps->vui.aspect_ratio_idc == SAR_Extended )
        {
            bs_write_u(b,16, sps->vui.sar_width);
            bs_write_u(b,16, sps->vui.sar_height);
        }
    }
    bs_write_u1(b, sps->vui.overscan_info_present_flag);
    if( sps->vui.overscan_info_present_flag )
    {
        bs_write_u1(b, sps->vui.overscan_appropriate_flag);
    }
    bs_write_u1(b, sps->vui.video_signal_type_present_flag);
    if( sps->vui.video_signal_type_present_flag )
    {
        bs_write_u(b,3, sps->vui.video_format);
        bs_write_u1(b, sps->vui.video_full_range_flag);
        bs_write_u1(b, sps->vui.colour_description_present_flag);
        if( sps->vui.colour_description_present_flag )
        {
            bs_write_u8(b, sps->vui.colour_primaries);
            bs_write_u8(b, sps->vui.transfer_characteristics);
            bs_write_u8(b, sps->vui.matrix_coefficients);
        }
    }
    bs_write_u1(b, sps->vui.chroma_loc_info_present_flag);
    if( sps->vui.chroma_loc_info_present_flag )
    {
        bs_write_ue(b, sps->vui.chroma_sample_loc_type_top_field);
        bs_write_ue(b, sps->vui.chroma_sample_loc_type_bottom_field);
    }
    bs_write_u1(b, sps->vui.timing_info_present_flag);
    if( sps->vui.timing_info_present_flag )
    {
        bs_write_u(b,32, sps->vui.num_units_in_tick);
        bs_write_u(b,32, sps->vui.time_scale);
        bs_write_u1(b, sps->vui.fixed_frame_rate_flag);
    }
    bs_write_u1(b, sps->vui.nal_hrd_parameters_present_flag);
    if( sps->vui.nal_hrd_parameters_present_flag )
    {
        write_hrd_parameters(h, b);
    }
    bs_write_u1(b, sps->vui.vcl_hrd_parameters_present_flag);
    if( sps->vui.vcl_hrd_parameters_present_flag )
    {
        write_hrd_parameters(h, b);
    }
    if( sps->vui.nal_hrd_parameters_present_flag || sps->vui.vcl_hrd_parameters_present_flag )
    {
        bs_write_u1(b, sps->vui.low_delay_hrd_flag);
    }
    bs_write_u1(b, sps->vui.pic_struct_present_flag);
    bs_write_u1(b, sps->vui.bitstream_restriction_flag);
    if( sps->vui.bitstream_restriction_flag )
    {
        bs_write_u1(b, sps->vui.motion_vectors_over_pic_boundaries_flag);
        bs_write_ue(b, sps->vui.max_bytes_per_pic_denom);
        bs_write_ue(b, sps->vui.max_bits_per_mb_denom);
        bs_write_ue(b, sps->vui.log2_max_mv_length_horizontal);
        bs_write_ue(b, sps->vui.log2_max_mv_length_vertical);
        bs_write_ue(b, sps->vui.num_reorder_frames);
        bs_write_ue(b, sps->vui.max_dec_frame_buffering);
    }
}

//Appendix E.1.2 HRD parameters syntax
void
write_hrd_parameters(h264_stream_t* h, bs_t* b)
{
    sps_t* sps = h->sps;
    int SchedSelIdx;

    bs_write_ue(b, sps->hrd.cpb_cnt_minus1);
    bs_write_u(b,4, sps->hrd.bit_rate_scale);
    bs_write_u(b,4, sps->hrd.cpb_size_scale);
    for( SchedSelIdx = 0; SchedSelIdx <= sps->hrd.cpb_cnt_minus1; SchedSelIdx++ )
    {
        bs_write_ue(b, sps->hrd.bit_rate_value_minus1[ SchedSelIdx ]);
        bs_write_ue(b, sps->hrd.cpb_size_value_minus1[ SchedSelIdx ]);
        bs_write_u1(b, sps->hrd.cbr_flag[ SchedSelIdx ]);
    }
    bs_write_u(b,5, sps->hrd.initial_cpb_removal_delay_length_minus1);
    bs_write_u(b,5, sps->hrd.cpb_removal_delay_length_minus1);
    bs_write_u(b,5, sps->hrd.dpb_output_delay_length_minus1);
    bs_write_u(b,5, sps->hrd.time_offset_length);
}

/*
UNIMPLEMENTED
//7.3.2.1.2 Sequence parameter set extension RBSP syntax
int write_seq_parameter_set_extension_rbsp(bs_t* b, sps_ext_t* sps_ext) {
    bs_write_ue(b, seq_parameter_set_id);
    bs_write_ue(b, aux_format_idc);
    if( aux_format_idc != 0 ) {
        bs_write_ue(b, bit_depth_aux_minus8);
        bs_write_u1(b, alpha_incr_flag);
        bs_write_u(v, alpha_opaque_value);
        bs_write_u(v, alpha_transparent_value);
    }
    bs_write_u1(b, additional_extension_flag);
    write_rbsp_trailing_bits();
}
*/

//7.3.2.2 Picture parameter set RBSP syntax
void write_pic_parameter_set_rbsp(h264_stream_t* h, bs_t* b) {
    pps_t* pps = h->pps;

    int i;
    int i_group;

    bs_write_ue(b, pps->pic_parameter_set_id);
    bs_write_ue(b, pps->seq_parameter_set_id);
    bs_write_u1(b, pps->entropy_coding_mode_flag);
    bs_write_u1(b, pps->pic_order_present_flag);
    bs_write_ue(b, pps->num_slice_groups_minus1);
    if( pps->num_slice_groups_minus1 > 0 )
    {
        bs_write_ue(b, pps->slice_group_map_type);
        if( pps->slice_group_map_type == 0 )
        {
            for( i_group = 0; i_group <= pps->num_slice_groups_minus1; i_group++ )
            {
                bs_write_ue(b, pps->run_length_minus1[ i_group ]);
            }
        }
        else if( pps->slice_group_map_type == 2 )
        {
            for( i_group = 0; i_group < pps->num_slice_groups_minus1; i_group++ )
            {
                bs_write_ue(b, pps->top_left[ i_group ]);
                bs_write_ue(b, pps->bottom_right[ i_group ]);
            }
        }
        else if( pps->slice_group_map_type == 3 ||
                 pps->slice_group_map_type == 4 ||
                 pps->slice_group_map_type == 5 )
        {
            bs_write_u1(b, pps->slice_group_change_direction_flag);
            bs_write_ue(b, pps->slice_group_change_rate_minus1);
        }
        else if( pps->slice_group_map_type == 6 )
        {
            bs_write_ue(b, pps->pic_size_in_map_units_minus1);
            for( i = 0; i <= pps->pic_size_in_map_units_minus1; i++ )
            {
                bs_write_u(b, ceil( log2( pps->num_slice_groups_minus1 + 1 ) ), pps->slice_group_id[ i ] ); // was u(v)
            }
        }
    }
    bs_write_ue(b, pps->num_ref_idx_l0_active_minus1);
    bs_write_ue(b, pps->num_ref_idx_l1_active_minus1);
    bs_write_u1(b, pps->weighted_pred_flag);
    bs_write_u(b,2, pps->weighted_bipred_idc);
    bs_write_se(b, pps->pic_init_qp_minus26);
    bs_write_se(b, pps->pic_init_qs_minus26);
    bs_write_se(b, pps->chroma_qp_index_offset);
    bs_write_u1(b, pps->deblocking_filter_control_present_flag);
    bs_write_u1(b, pps->constrained_intra_pred_flag);
    bs_write_u1(b, pps->redundant_pic_cnt_present_flag);
    if( more_rbsp_data(h, b) )
    {
        bs_write_u1(b, pps->transform_8x8_mode_flag);
        bs_write_u1(b, pps->pic_scaling_matrix_present_flag);
        if( pps->pic_scaling_matrix_present_flag )
        {
            for( i = 0; i < 6 + 2* pps->transform_8x8_mode_flag; i++ )
            {
                bs_write_u1(b, pps->pic_scaling_list_present_flag[ i ]);
                if( pps->pic_scaling_list_present_flag[ i ] )
                {
                    if( i < 6 )
                    {
                        write_scaling_list( b, pps->ScalingList4x4[ i ], 16,
                                      pps->UseDefaultScalingMatrix4x4Flag[ i ] );
                    }
                    else
                    {
                        write_scaling_list( b, pps->ScalingList8x8[ i - 6 ], 64,
                                      pps->UseDefaultScalingMatrix8x8Flag[ i - 6 ] );
                    }
                }
            }
        }
        bs_write_se(b, pps->second_chroma_qp_index_offset);
    }
    write_rbsp_trailing_bits(h, b);
}

/*
// UNIMPLEMENTED
//7.3.2.3 Supplemental enhancement information RBSP syntax
write_sei_rbsp( ) {
    do {
        write_sei_message( );
    } while( more_rbsp_data( ) );
    write_rbsp_trailing_bits( );
}

//7.3.2.3.1 Supplemental enhancement information message syntax
write_sei_message( ) {
    payloadType = 0;
    while( next_bits( 8 ) == 0xFF ) {
        bs_write_f(8, ff_byte); // equal to 0xFF
        payloadType += 255;
    }
    bs_write_u8(b, last_payload_type_byte);
    payloadType += last_payload_type_byte;
    payloadSize = 0;
    while( next_bits( 8 ) == 0xFF ) {
        bs_write_f(8, ff_byte); // equal to 0xFF
        payloadSize += 255;
    }
    bs_write_u8(b, last_payload_size_byte);
    payloadSize += last_payload_size_byte;
    sei_payload( payloadType, payloadSize );
}
*/

//7.3.2.4 Access unit delimiter RBSP syntax
void write_access_unit_delimiter_rbsp(h264_stream_t* h, bs_t* b)
{
    int primary_pic_type; //FIXME
    bs_write_u(b,3, primary_pic_type);
    write_rbsp_trailing_bits(h, b);
}

//7.3.2.5 End of sequence RBSP syntax
void write_end_of_seq_rbsp(h264_stream_t* h, bs_t* b)
{
}

//7.3.2.6 End of stream RBSP syntax
void write_end_of_stream_rbsp(h264_stream_t* h, bs_t* b)
{
}

//7.3.2.7 Filler data RBSP syntax
void write_filler_data_rbsp(h264_stream_t* h, bs_t* b)
{
    int ff_byte; //FIXME
    while( next_bits(b, 8) == 0xFF )
    {
        bs_write_f(b,8, ff_byte);  // equal to 0xFF
    }
    write_rbsp_trailing_bits(h, b);
}

//7.3.2.8 Slice layer without partitioning RBSP syntax
void write_slice_layer_rbsp(h264_stream_t* h, bs_t* b)
{
    write_slice_header(h, b);
    //slice_data( ); /* all categories of slice_data( ) syntax */
    write_rbsp_slice_trailing_bits(h, b);
}

/*
// UNIMPLEMENTED
//7.3.2.9.1 Slice data partition A RBSP syntax
slice_data_partition_a_layer_rbsp( )
{
    write_slice_header( );             // only category 2
    bs_write_ue(b, slice_id)
    write_slice_data( );               // only category 2
    write_rbsp_slice_trailing_bits( ); // only category 2
}

//7.3.2.9.2 Slice data partition B RBSP syntax
slice_data_partition_b_layer_rbsp( )
{
    bs_write_ue(b, slice_id);    // only category 3
    if( redundant_pic_cnt_present_flag )
        bs_write_ue(b, redundant_pic_cnt);
    write_slice_data( );               // only category 3
    write_rbsp_slice_trailing_bits( ); // only category 3
}

//7.3.2.9.3 Slice data partition C RBSP syntax
slice_data_partition_c_layer_rbsp( )
{
    bs_write_ue(b, slice_id);    // only category 4
    if( redundant_pic_cnt_present_flag )
        bs_write_ue(b, redundant_pic_cnt);
    write_slice_data( );               // only category 4
    rbsp_slice_trailing_bits( ); // only category 4
}
*/

//7.3.2.10 RBSP slice trailing bits syntax
void write_rbsp_slice_trailing_bits(h264_stream_t* h, bs_t* b)
{
    write_rbsp_trailing_bits(h, b);
    int cabac_zero_word;
    if( h->pps->entropy_coding_mode_flag )
    {
        /*
        // 9.3.4.6 Byte stuffing process (informative)
        // NOTE do not write any cabac_zero_word for now - this appears to be optional
        while( more_rbsp_trailing_data(h, b) )
        {
            bs_write_f(b,16, cabac_zero_word); // equal to 0x0000
        }
        */
    }
}

//7.3.2.11 RBSP trailing bits syntax
void write_rbsp_trailing_bits(h264_stream_t* h, bs_t* b)
{
    int rbsp_stop_one_bit = 1;
    int rbsp_alignment_zero_bit = 0;
    if( !bs_byte_aligned(b) )
    {
        bs_write_f(b,1, rbsp_stop_one_bit); // equal to 1
        while( !bs_byte_aligned(b) )
        {
            bs_write_f(b,1, rbsp_alignment_zero_bit); // equal to 0
        }
    }
}

//7.3.3 Slice header syntax
void write_slice_header(h264_stream_t* h, bs_t* b)
{
    slice_header_t* sh = h->sh;
    sps_t* sps = h->sps;
    pps_t* pps = h->pps;
    nal_t* nal = h->nal;

    //DBG_START
    //h2->sh = (slice_header_t*)malloc(sizeof(slice_header_t));

    bs_write_ue(b, sh->first_mb_in_slice);
    bs_write_ue(b, sh->slice_type);
    bs_write_ue(b, sh->pic_parameter_set_id);
    bs_write_u(b, sps->log2_max_frame_num_minus4 + 4, sh->frame_num ); // was u(v)
    if( !sps->frame_mbs_only_flag )
    {
        bs_write_u1(b, sh->field_pic_flag);
        if( sh->field_pic_flag )
        {
            bs_write_u1(b, sh->bottom_field_flag);
        }
    }
    if( nal->nal_unit_type == 5 )
    {
        bs_write_ue(b, sh->idr_pic_id);
    }
    if( sps->pic_order_cnt_type == 0 )
    {
        bs_write_u(b, sps->log2_max_pic_order_cnt_lsb_minus4 + 4, sh->pic_order_cnt_lsb ); // was u(v)
        if( pps->pic_order_present_flag && !sh->field_pic_flag )
        {
            bs_write_se(b, sh->delta_pic_order_cnt_bottom);
        }
    }
    if( sps->pic_order_cnt_type == 1 && !sps->delta_pic_order_always_zero_flag )
    {
        bs_write_se(b, sh->delta_pic_order_cnt[ 0 ]);
        if( pps->pic_order_present_flag && !sh->field_pic_flag )
        {
            bs_write_se(b, sh->delta_pic_order_cnt[ 1 ]);
        }
    }
    if( pps->redundant_pic_cnt_present_flag )
    {
        bs_write_ue(b, sh->redundant_pic_cnt);
    }
    if( sh->slice_type == SH_SLICE_TYPE_B )
    {
        bs_write_u1(b, sh->direct_spatial_mv_pred_flag);
    }
    if( sh->slice_type == SH_SLICE_TYPE_P || sh->slice_type == SH_SLICE_TYPE_SP || sh->slice_type == SH_SLICE_TYPE_B )
    {
        bs_write_u1(b, sh->num_ref_idx_active_override_flag);
        if( sh->num_ref_idx_active_override_flag )
        {
            bs_write_ue(b, sh->num_ref_idx_l0_active_minus1); // FIXME does this modify the pps?
            if( sh->slice_type == SH_SLICE_TYPE_B )
            {
                bs_write_ue(b, sh->num_ref_idx_l1_active_minus1);
            }
        }
    }
    write_ref_pic_list_reordering(h, b);
    if( ( pps->weighted_pred_flag && ( sh->slice_type == SH_SLICE_TYPE_P || sh->slice_type == SH_SLICE_TYPE_SP ) ) ||
        ( pps->weighted_bipred_idc == 1 && sh->slice_type == SH_SLICE_TYPE_B ) )
    {
        write_pred_weight_table(h, b);
    }
    if( nal->nal_ref_idc != 0 )
    {
        write_dec_ref_pic_marking(h, b);
    }
    if( pps->entropy_coding_mode_flag && sh->slice_type != SH_SLICE_TYPE_I && sh->slice_type != SH_SLICE_TYPE_SI )
    {
        bs_write_ue(b, sh->cabac_init_idc);
    }
    bs_write_se(b, sh->slice_qp_delta);
    if( sh->slice_type == SH_SLICE_TYPE_SP || sh->slice_type == SH_SLICE_TYPE_SI )
    {
        if( sh->slice_type == SH_SLICE_TYPE_SP )
        {
            bs_write_u1(b, sh->sp_for_switch_flag);
        }
        bs_write_se(b, sh->slice_qs_delta);
    }
    if( pps->deblocking_filter_control_present_flag )
    {
        bs_write_ue(b, sh->disable_deblocking_filter_idc);
        if( sh->disable_deblocking_filter_idc != 1 )
        {
            bs_write_se(b, sh->slice_alpha_c0_offset_div2);
            bs_write_se(b, sh->slice_beta_offset_div2);
        }
    }
    if( pps->num_slice_groups_minus1 > 0 &&
        pps->slice_group_map_type >= 3 && pps->slice_group_map_type <= 5)
    {
        bs_write_u(b, ceil( log2( pps->pic_size_in_map_units_minus1 +  
                                  pps->slice_group_change_rate_minus1 + 1 ) ),
                   sh->slice_group_change_cycle ); // was u(v) // FIXME add 2?
    }

    //free(h2->sh);
    //DBG_END
}

//7.3.3.1 Reference picture list reordering syntax
void write_ref_pic_list_reordering(h264_stream_t* h, bs_t* b)
{
    slice_header_t* sh = h->sh;

    if( sh->slice_type != SH_SLICE_TYPE_I && sh->slice_type != SH_SLICE_TYPE_SI )
    {
        bs_write_u1(b, sh->rplr.ref_pic_list_reordering_flag_l0);
        if( sh->rplr.ref_pic_list_reordering_flag_l0 )
        {
            do
            {
                bs_write_ue(b, sh->rplr.reordering_of_pic_nums_idc);
                if( sh->rplr.reordering_of_pic_nums_idc == 0 ||
                    sh->rplr.reordering_of_pic_nums_idc == 1 )
                {
                    bs_write_ue(b, sh->rplr.abs_diff_pic_num_minus1);
                }
                else if( sh->rplr.reordering_of_pic_nums_idc == 2 )
                {
                    bs_write_ue(b, sh->rplr.long_term_pic_num);
                }
            } while( sh->rplr.reordering_of_pic_nums_idc != 3 );
        }
    }
    if( sh->slice_type == SH_SLICE_TYPE_B )
    {
        bs_write_u1(b, sh->rplr.ref_pic_list_reordering_flag_l1);
        if( sh->rplr.ref_pic_list_reordering_flag_l1 )
        {
            do
            {
                bs_write_ue(b, sh->rplr.reordering_of_pic_nums_idc);
                if( sh->rplr.reordering_of_pic_nums_idc == 0 ||
                    sh->rplr.reordering_of_pic_nums_idc == 1 )
                {
                    bs_write_ue(b, sh->rplr.abs_diff_pic_num_minus1);
                }
                else if( sh->rplr.reordering_of_pic_nums_idc == 2 )
                {
                    bs_write_ue(b, sh->rplr.long_term_pic_num);
                }
            } while( sh->rplr.reordering_of_pic_nums_idc != 3 );
        }
    }
}

//7.3.3.2 Prediction weight table syntax
void write_pred_weight_table(h264_stream_t* h, bs_t* b)
{
    slice_header_t* sh = h->sh;
    sps_t* sps = h->sps;
    pps_t* pps = h->pps;

    int i, j;

    bs_write_ue(b, sh->pwt.luma_log2_weight_denom);
    if( sps->chroma_format_idc != 0 )
    {
        bs_write_ue(b, sh->pwt.chroma_log2_weight_denom);
    }
    for( i = 0; i <= pps->num_ref_idx_l0_active_minus1; i++ )
    {
        bs_write_u1(b, sh->pwt.luma_weight_l0_flag);
        if( sh->pwt.luma_weight_l0_flag )
        {
            bs_write_se(b, sh->pwt.luma_weight_l0[ i ]);
            bs_write_se(b, sh->pwt.luma_offset_l0[ i ]);
        }
        if ( sps->chroma_format_idc != 0 )
        {
            bs_write_u1(b, sh->pwt.chroma_weight_l0_flag);
            if( sh->pwt.chroma_weight_l0_flag )
            {
                for( j =0; j < 2; j++ )
                {
                    bs_write_se(b, sh->pwt.chroma_weight_l0[ i ][ j ]);
                    bs_write_se(b, sh->pwt.chroma_offset_l0[ i ][ j ]);
                }
            }
        }
    }
    if( sh->slice_type == SH_SLICE_TYPE_B )
    {
        for( i = 0; i <= pps->num_ref_idx_l1_active_minus1; i++ )
        {
            bs_write_u1(b, sh->pwt.luma_weight_l1_flag);
            if( sh->pwt.luma_weight_l1_flag )
            {
                bs_write_se(b, sh->pwt.luma_weight_l1[ i ]);
                bs_write_se(b, sh->pwt.luma_offset_l1[ i ]);
            }
            if( sps->chroma_format_idc != 0 )
            {
                bs_write_u1(b, sh->pwt.chroma_weight_l1_flag);
                if( sh->pwt.chroma_weight_l1_flag )
                {
                    for( j = 0; j < 2; j++ )
                    {
                        bs_write_se(b, sh->pwt.chroma_weight_l1[ i ][ j ]);
                        bs_write_se(b, sh->pwt.chroma_offset_l1[ i ][ j ]);
                    }
                }
            }
        }
    }
}

//7.3.3.3 Decoded reference picture marking syntax
void write_dec_ref_pic_marking(h264_stream_t* h, bs_t* b)
{
    slice_header_t* sh = h->sh;

    if( h->nal->nal_unit_type == 5 )
    {
        bs_write_u1(b, sh->drpm.no_output_of_prior_pics_flag);
        bs_write_u1(b, sh->drpm.long_term_reference_flag);
    }
    else
    {
        bs_write_u1(b, sh->drpm.adaptive_ref_pic_marking_mode_flag);
        if( sh->drpm.adaptive_ref_pic_marking_mode_flag )
        {
            do
            {
                bs_write_ue(b, sh->drpm.memory_management_control_operation);
                if( sh->drpm.memory_management_control_operation == 1 ||
                    sh->drpm.memory_management_control_operation == 3 )
                {
                    bs_write_ue(b, sh->drpm.difference_of_pic_nums_minus1);
                }
                if(sh->drpm.memory_management_control_operation == 2 )
                {
                    bs_write_ue(b, sh->drpm.long_term_pic_num);
                }
                if( sh->drpm.memory_management_control_operation == 3 ||
                    sh->drpm.memory_management_control_operation == 6 )
                {
                    bs_write_ue(b, sh->drpm.long_term_frame_idx);
                }
                if( sh->drpm.memory_management_control_operation == 4 )
                {
                    bs_write_ue(b, sh->drpm.max_long_term_frame_idx_plus1);
                }
            } while( sh->drpm.memory_management_control_operation != 0 );
        }
    }
}

/*
// UNIMPLEMENTED
//7.3.4 Slice data syntax
slice_data( )
{
    if( pps->entropy_coding_mode_flag )
        while( !byte_aligned( ) )
            bs_write_f(1, cabac_alignment_one_bit);
    CurrMbAddr = first_mb_in_slice * ( 1 + MbaffFrameFlag );
    moreDataFlag = 1;
    prevMbSkipped = 0;
    do {
        if( sh->slice_type != SH_SLICE_TYPE_I && ssh->lice_type != SH_SLICE_TYPE_SI )
            if( !pps->entropy_coding_mode_flag ) {
                bs_write_ue(b, mb_skip_run);
                prevMbSkipped = ( mb_skip_run > 0 );
                for( i=0; i<mb_skip_run; i++ )
                    CurrMbAddr = NextMbAddress( CurrMbAddr );
                moreDataFlag = more_rbsp_data( );
            } else {
                bs_write_ae(v, mb_skip_flag);
                moreDataFlag = !mb_skip_flag;
            }
        if( moreDataFlag ) {
            if( MbaffFrameFlag && ( CurrMbAddr % 2 == 0 ||
                                    ( CurrMbAddr % 2 == 1 && prevMbSkipped ) ) )
                bs_write_u1(b) | bs_write_ae(v, mb_field_decoding_flag);
            macroblock_layer( );
        }
        if( !pps->entropy_coding_mode_flag )
            moreDataFlag = more_rbsp_data( );
        else {
            if( sh->slice_type != SH_SLICE_TYPE_I && sh->slice_type != SH_SLICE_TYPE_SI )
                prevMbSkipped = mb_skip_flag;
            if( MbaffFrameFlag && CurrMbAddr % 2 == 0 )
                moreDataFlag = 1;
            else {
                bs_write_ae(v, end_of_slice_flag);
                moreDataFlag = !end_of_slice_flag;
            }
        }
        CurrMbAddr = NextMbAddress( CurrMbAddr );
    } while( moreDataFlag );
}
*/




/************** debug **************/

void debug_sps(sps_t* sps)
{
    printf("======= SPS =======\n");
    printf(" profile_idc : %02x \n", sps->profile_idc );
    printf(" constraint_set0_flag : %02x \n", sps->constraint_set0_flag );
    printf(" constraint_set1_flag : %02x \n", sps->constraint_set1_flag );
    printf(" constraint_set2_flag : %02x \n", sps->constraint_set2_flag );
    printf(" constraint_set3_flag : %02x \n", sps->constraint_set3_flag );
    printf(" reserved_zero_4bits : %02x \n", sps->reserved_zero_4bits );
    printf(" level_idc : %02x \n", sps->level_idc );
    printf(" seq_parameter_set_id : %02x \n", sps->seq_parameter_set_id );
    printf(" chroma_format_idc : %02x \n", sps->chroma_format_idc );
    printf(" residual_colour_transform_flag : %02x \n", sps->residual_colour_transform_flag );
    printf(" bit_depth_luma_minus8 : %02x \n", sps->bit_depth_luma_minus8 );
    printf(" bit_depth_chroma_minus8 : %02x \n", sps->bit_depth_chroma_minus8 );
    printf(" qpprime_y_zero_transform_bypass_flag : %02x \n", sps->qpprime_y_zero_transform_bypass_flag );
    printf(" seq_scaling_matrix_present_flag : %02x \n", sps->seq_scaling_matrix_present_flag );
    //  int seq_scaling_list_present_flag[8];
    //  void* ScalingList4x4[6];
    //  int UseDefaultScalingMatrix4x4Flag[6];
    //  void* ScalingList8x8[2];
    //  int UseDefaultScalingMatrix8x8Flag[2];
    printf(" log2_max_frame_num_minus4 : %02x \n", sps->log2_max_frame_num_minus4 );
    printf(" pic_order_cnt_type : %02x \n", sps->pic_order_cnt_type );
      printf("   log2_max_pic_order_cnt_lsb_minus4 : %02x \n", sps->log2_max_pic_order_cnt_lsb_minus4 );
      printf("   delta_pic_order_always_zero_flag : %02x \n", sps->delta_pic_order_always_zero_flag );
      printf("   offset_for_non_ref_pic : %02x \n", sps->offset_for_non_ref_pic );
      printf("   offset_for_top_to_bottom_field : %02x \n", sps->offset_for_top_to_bottom_field );
      printf("   num_ref_frames_in_pic_order_cnt_cycle : %02x \n", sps->num_ref_frames_in_pic_order_cnt_cycle );
    //  int offset_for_ref_frame[256];
    printf(" num_ref_frames : %02x \n", sps->num_ref_frames );
    printf(" gaps_in_frame_num_value_allowed_flag : %02x \n", sps->gaps_in_frame_num_value_allowed_flag );
    printf(" pic_width_in_mbs_minus1 : %02x \n", sps->pic_width_in_mbs_minus1 );
    printf(" pic_height_in_map_units_minus1 : %02x \n", sps->pic_height_in_map_units_minus1 );
    printf(" frame_mbs_only_flag : %02x \n", sps->frame_mbs_only_flag );
    printf(" mb_adaptive_frame_field_flag : %02x \n", sps->mb_adaptive_frame_field_flag );
    printf(" direct_8x8_inference_flag : %02x \n", sps->direct_8x8_inference_flag );
    printf(" frame_cropping_flag : %02x \n", sps->frame_cropping_flag );
      printf("   frame_crop_left_offset : %02x \n", sps->frame_crop_left_offset );
      printf("   frame_crop_right_offset : %02x \n", sps->frame_crop_right_offset );
      printf("   frame_crop_top_offset : %02x \n", sps->frame_crop_top_offset );
      printf("   frame_crop_bottom_offset : %02x \n", sps->frame_crop_bottom_offset );
    printf(" vui_parameters_present_flag : %02x \n", sps->vui_parameters_present_flag );

    printf("=== VUI ===\n");
    printf(" aspect_ratio_info_present_flag : %02x \n", sps->vui.aspect_ratio_info_present_flag );
      printf("   aspect_ratio_idc : %02x \n", sps->vui.aspect_ratio_idc );
        printf("     sar_width : %02x \n", sps->vui.sar_width );
        printf("     sar_height : %02x \n", sps->vui.sar_height );
    printf(" overscan_info_present_flag : %02x \n", sps->vui.overscan_info_present_flag );
      printf("   overscan_appropriate_flag : %02x \n", sps->vui.overscan_appropriate_flag );
    printf(" video_signal_type_present_flag : %02x \n", sps->vui.video_signal_type_present_flag );
      printf("   video_format : %02x \n", sps->vui.video_format );
      printf("   video_full_range_flag : %02x \n", sps->vui.video_full_range_flag );
      printf("   colour_description_present_flag : %02x \n", sps->vui.colour_description_present_flag );
        printf("     colour_primaries : %02x \n", sps->vui.colour_primaries );
        printf("   transfer_characteristics : %02x \n", sps->vui.transfer_characteristics );
        printf("   matrix_coefficients : %02x \n", sps->vui.matrix_coefficients );
    printf(" chroma_loc_info_present_flag : %02x \n", sps->vui.chroma_loc_info_present_flag );
      printf("   chroma_sample_loc_type_top_field : %02x \n", sps->vui.chroma_sample_loc_type_top_field );
      printf("   chroma_sample_loc_type_bottom_field : %02x \n", sps->vui.chroma_sample_loc_type_bottom_field );
    printf(" timing_info_present_flag : %02x \n", sps->vui.timing_info_present_flag );
      printf("   num_units_in_tick : %02x \n", sps->vui.num_units_in_tick );
      printf("   time_scale : %02x \n", sps->vui.time_scale );
      printf("   fixed_frame_rate_flag : %02x \n", sps->vui.fixed_frame_rate_flag );
    printf(" nal_hrd_parameters_present_flag : %02x \n", sps->vui.nal_hrd_parameters_present_flag );
    printf(" vcl_hrd_parameters_present_flag : %02x \n", sps->vui.vcl_hrd_parameters_present_flag );
      printf("   low_delay_hrd_flag : %02x \n", sps->vui.low_delay_hrd_flag );
    printf(" pic_struct_present_flag : %02x \n", sps->vui.pic_struct_present_flag );
    printf(" bitstream_restriction_flag : %02x \n", sps->vui.bitstream_restriction_flag );
      printf("   motion_vectors_over_pic_boundaries_flag : %02x \n", sps->vui.motion_vectors_over_pic_boundaries_flag );
      printf("   max_bytes_per_pic_denom : %02x \n", sps->vui.max_bytes_per_pic_denom );
      printf("   max_bits_per_mb_denom : %02x \n", sps->vui.max_bits_per_mb_denom );
      printf("   log2_max_mv_length_horizontal : %02x \n", sps->vui.log2_max_mv_length_horizontal );
      printf("   log2_max_mv_length_vertical : %02x \n", sps->vui.log2_max_mv_length_vertical );
      printf("   num_reorder_frames : %02x \n", sps->vui.num_reorder_frames );
      printf("   max_dec_frame_buffering : %02x \n", sps->vui.max_dec_frame_buffering );

    printf("=== HRD ===\n");
    printf(" cpb_cnt_minus1 : %02x \n", sps->hrd.cpb_cnt_minus1 );
    printf(" bit_rate_scale : %02x \n", sps->hrd.bit_rate_scale );
    printf(" cpb_size_scale : %02x \n", sps->hrd.cpb_size_scale );
    //  printf("bit_rate_value_minus1[32] : %02x \n", sps->hrd.bit_rate_value_minus1[32] ); // up to cpb_cnt_minus1, which is <= 31
    //  printf("cpb_size_value_minus1[32] : %02x \n", sps->hrd.cpb_size_value_minus1[32] );
    //  printf("cbr_flag[32] : %02x \n", sps->hrd.cbr_flag[32] );
    printf(" initial_cpb_removal_delay_length_minus1 : %02x \n", sps->hrd.initial_cpb_removal_delay_length_minus1 );
    printf(" cpb_removal_delay_length_minus1 : %02x \n", sps->hrd.cpb_removal_delay_length_minus1 );
    printf(" dpb_output_delay_length_minus1 : %02x \n", sps->hrd.dpb_output_delay_length_minus1 );
    printf(" time_offset_length : %02x \n", sps->hrd.time_offset_length );
}


void debug_pps(pps_t* pps)
{
    printf("======= PPS =======\n");
    printf(" pic_parameter_set_id : %02x \n", pps->pic_parameter_set_id );
    printf(" seq_parameter_set_id : %02x \n", pps->seq_parameter_set_id );
    printf(" entropy_coding_mode_flag : %02x \n", pps->entropy_coding_mode_flag );
    printf(" pic_order_present_flag : %02x \n", pps->pic_order_present_flag );
    printf(" num_slice_groups_minus1 : %02x \n", pps->num_slice_groups_minus1 );
    printf(" slice_group_map_type : %02x \n", pps->slice_group_map_type );
    //  int run_length_minus1[8]; // up to num_slice_groups_minus1, which is <= 7 in Baseline and Extended, 0 otheriwse
    //  int top_left[8];
    //  int bottom_right[8];
    //  int slice_group_change_direction_flag;
    //  int slice_group_change_rate_minus1;
    //  int pic_size_in_map_units_minus1;
    //  int slice_group_id[256]; // FIXME what size?
    printf(" num_ref_idx_l0_active_minus1 : %02x \n", pps->num_ref_idx_l0_active_minus1 );
    printf(" num_ref_idx_l1_active_minus1 : %02x \n", pps->num_ref_idx_l1_active_minus1 );
    printf(" weighted_pred_flag : %02x \n", pps->weighted_pred_flag );
    printf(" weighted_bipred_idc : %02x \n", pps->weighted_bipred_idc );
    printf(" pic_init_qp_minus26 : %02x \n", pps->pic_init_qp_minus26 );
    printf(" pic_init_qs_minus26 : %02x \n", pps->pic_init_qs_minus26 );
    printf(" chroma_qp_index_offset : %02x \n", pps->chroma_qp_index_offset );
    printf(" deblocking_filter_control_present_flag : %02x \n", pps->deblocking_filter_control_present_flag );
    printf(" constrained_intra_pred_flag : %02x \n", pps->constrained_intra_pred_flag );
    printf(" redundant_pic_cnt_present_flag : %02x \n", pps->redundant_pic_cnt_present_flag );
    printf(" transform_8x8_mode_flag : %02x \n", pps->transform_8x8_mode_flag );
    printf(" pic_scaling_matrix_present_flag : %02x \n", pps->pic_scaling_matrix_present_flag );
    //  int pic_scaling_list_present_flag[8];
    //  void* ScalingList4x4[6];
    //  int UseDefaultScalingMatrix4x4Flag[6];
    //  void* ScalingList8x8[2];
    //  int UseDefaultScalingMatrix8x8Flag[2];
    printf(" second_chroma_qp_index_offset : %02x \n", pps->second_chroma_qp_index_offset );
}

void debug_slice_header(slice_header_t* sh)
{
    printf("======= Slice Header =======\n");
    printf(" first_mb_in_slice : %02x \n", sh->first_mb_in_slice );
    const char* slice_type_name;
    if (sh->slice_type == SH_SLICE_TYPE_I) { slice_type_name = "I Frame"; }
    else if (sh->slice_type == SH_SLICE_TYPE_P) { slice_type_name = "P Frame"; }
    else if (sh->slice_type == SH_SLICE_TYPE_B) { slice_type_name = "B Frame"; }
    else { slice_type_name = "?"; }
    printf(" slice_type : %02x ( %s ) \n", sh->slice_type, slice_type_name );
    printf(" pic_parameter_set_id : %02x \n", sh->pic_parameter_set_id );
    printf(" frame_num : %02x \n", sh->frame_num );
    printf(" field_pic_flag : %02x \n", sh->field_pic_flag );
      printf(" bottom_field_flag : %02x \n", sh->bottom_field_flag );
    printf(" idr_pic_id : %02x \n", sh->idr_pic_id );
    printf(" pic_order_cnt_lsb : %02x \n", sh->pic_order_cnt_lsb );
    printf(" delta_pic_order_cnt_bottom : %02x \n", sh->delta_pic_order_cnt_bottom );
    // int delta_pic_order_cnt[ 2 ];
    printf(" redundant_pic_cnt : %02x \n", sh->redundant_pic_cnt );
    printf(" direct_spatial_mv_pred_flag : %02x \n", sh->direct_spatial_mv_pred_flag );
    printf(" num_ref_idx_active_override_flag : %02x \n", sh->num_ref_idx_active_override_flag );
    printf(" num_ref_idx_l0_active_minus1 : %02x \n", sh->num_ref_idx_l0_active_minus1 );
    printf(" num_ref_idx_l1_active_minus1 : %02x \n", sh->num_ref_idx_l1_active_minus1 );
    printf(" cabac_init_idc : %02x \n", sh->cabac_init_idc );
    printf(" slice_qp_delta : %02x \n", sh->slice_qp_delta );
    printf(" sp_for_switch_flag : %02x \n", sh->sp_for_switch_flag );
    printf(" slice_qs_delta : %02x \n", sh->slice_qs_delta );
    printf(" disable_deblocking_filter_idc : %02x \n", sh->disable_deblocking_filter_idc );
    printf(" slice_alpha_c0_offset_div2 : %02x \n", sh->slice_alpha_c0_offset_div2 );
    printf(" slice_beta_offset_div2 : %02x \n", sh->slice_beta_offset_div2 );
    printf(" slice_group_change_cycle : %02x \n", sh->slice_group_change_cycle );

    printf("=== Prediction Weight Table ===\n");
        printf(" luma_log2_weight_denom : %02x \n", sh->pwt.luma_log2_weight_denom );
        printf(" chroma_log2_weight_denom : %02x \n", sh->pwt.chroma_log2_weight_denom );
        printf(" luma_weight_l0_flag : %02x \n", sh->pwt.luma_weight_l0_flag );
        // int luma_weight_l0[64];
        // int luma_offset_l0[64];
        printf(" chroma_weight_l0_flag : %02x \n", sh->pwt.chroma_weight_l0_flag );
        // int chroma_weight_l0[64][2];
        // int chroma_offset_l0[64][2];
        printf(" luma_weight_l1_flag : %02x \n", sh->pwt.luma_weight_l1_flag );
        // int luma_weight_l1[64];
        // int luma_offset_l1[64];
        printf(" chroma_weight_l1_flag : %02x \n", sh->pwt.chroma_weight_l1_flag );
        // int chroma_weight_l1[64][2];
        // int chroma_offset_l1[64][2];

    printf("=== Ref Pic List Reordering ===\n");
        printf(" ref_pic_list_reordering_flag_l0 : %02x \n", sh->rplr.ref_pic_list_reordering_flag_l0 );
        printf(" ref_pic_list_reordering_flag_l1 : %02x \n", sh->rplr.ref_pic_list_reordering_flag_l1 );
        // int reordering_of_pic_nums_idc;
        // int abs_diff_pic_num_minus1;
        // int long_term_pic_num;

    printf("=== Decoded Ref Pic Marking ===\n");
        printf(" no_output_of_prior_pics_flag : %02x \n", sh->drpm.no_output_of_prior_pics_flag );
        printf(" long_term_reference_flag : %02x \n", sh->drpm.long_term_reference_flag );
        printf(" adaptive_ref_pic_marking_mode_flag : %02x \n", sh->drpm.adaptive_ref_pic_marking_mode_flag );
        // int memory_management_control_operation;
        // int difference_of_pic_nums_minus1;
        // int long_term_pic_num;
        // int long_term_frame_idx;
        // int max_long_term_frame_idx_plus1;

}

/**
 Print the contents of a NAL unit to standard output.
 The NAL which is printed out has a type determined by nal and data which comes from other fields within h depending on its type.
 @param[in]      h          the stream object
 @param[in]      nal        the nal unit
 */
void debug_nal(h264_stream_t* h, nal_t* nal)
{
    printf("==================== NAL ====================\n");
    printf(" forbidden_zero_bit : %02x \n", nal->forbidden_zero_bit );
    printf(" nal_ref_idc : %02x \n", nal->nal_ref_idc );
    printf(" nal_unit_type : %02x \n", nal->nal_unit_type );
    //uint8_t* rbsp_buf;
    //int rbsp_size;

    if( nal->nal_unit_type == 0) { }                                 //  0    Unspecified
    else if( nal->nal_unit_type == 1) { debug_slice_header(h->sh); }       //  1    Coded slice of a non-IDR picture
    else if( nal->nal_unit_type == 5) { debug_slice_header(h->sh); }       //  5    Coded slice of an IDR picture
    else if( nal->nal_unit_type == 7) { debug_sps(h->sps); } //  7    Sequence parameter set
    else if( nal->nal_unit_type == 8) { debug_pps(h->pps); } //  8    Picture parameter set
}

void debug_bytes(uint8_t* buf, int len)
{
    int i;
    for (i = 0; i < len; i++)
    {
        printf("%02X ", buf[i]);
        if ((i+1) % 16 == 0) { printf ("\n"); }
    }
    printf("\n");
}

void debug_bs(bs_t* b)
{
    bs_t* b2 = (bs_t*)malloc(sizeof(bs_t));
    bs_init(b2, b->start, b->end - b->start);

    while (b2->p < b->p || 
           (b2->p == b->p && b2->bits_left > b->bits_left))
    {
        printf("%d", bs_read_u1(b2));
        if (b2->bits_left == 8) { printf(" "); }
    }

    free(b2);
}
