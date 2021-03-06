module sddk
    use, intrinsic :: ISO_C_BINDING

    interface

        subroutine sddk_create_fft_grid(dims, fft_grid_id)&
            &bind(C, name="sddk_create_fft_grid")
            integer,                 intent(in)  :: dims(3)
            integer,                 intent(out) :: fft_grid_id
        end subroutine

        subroutine sddk_delete_fft_grid(fft_grid_id)&
            &bind(C, name="sddk_delete_fft_grid")
            integer,                 intent(in)  :: fft_grid_id
        end subroutine

        subroutine sddk_create_fft(fft_grid_id, fcomm, fft_id)&
            &bind(C, name="sddk_create_fft")
            integer,                 intent(in)  :: fft_grid_id
            integer,                 intent(in)  :: fcomm
            integer,                 intent(out) :: fft_id
        end subroutine

        subroutine sddk_delete_fft(fft_id)&
            &bind(C, name="sddk_delete_fft")
            integer,                 intent(in)  :: fft_id
        end subroutine

        subroutine sddk_create_gvec(vk, b1, b2, b3, gmax, reduce_gvec, comm, comm_fft, gvec_id)&
            &bind(C, name="sddk_create_gvec")
            real(8),                 intent(in)  :: vk(3)
            real(8),                 intent(in)  :: b1(3)
            real(8),                 intent(in)  :: b2(3)
            real(8),                 intent(in)  :: b3(3)
            real(8),                 intent(in)  :: gmax
            integer,                 intent(in)  :: reduce_gvec
            integer,                 intent(in)  :: comm
            integer,                 intent(in)  :: comm_fft
            integer,                 intent(out) :: gvec_id
        end subroutine

        subroutine sddk_delete_gvec(gvec_id)&
            &bind(C, name="sddk_delete_gvec")
            integer,                 intent(in)  :: gvec_id
        end subroutine

        subroutine sddk_get_gvec_count(gvec_id, rank, gvec_count)&
            &bind(C, name="sddk_get_gvec_count")
            integer,                 intent(in)  :: gvec_id
            integer,                 intent(in)  :: rank
            integer,                 intent(out) :: gvec_count
        end subroutine

        subroutine sddk_get_gvec_offset(gvec_id, rank, gvec_offset)&
            &bind(C, name="sddk_get_gvec_offset")
            integer,                 intent(in)  :: gvec_id
            integer,                 intent(in)  :: rank
            integer,                 intent(out) :: gvec_offset
        end subroutine

        subroutine sddk_get_gvec_count_fft(gvec_id, gvec_count)&
            &bind(C, name="sddk_get_gvec_count_fft")
            integer,                 intent(in)  :: gvec_id
            integer,                 intent(out) :: gvec_count
        end subroutine

        subroutine sddk_get_gvec_offset_fft(gvec_id, gvec_offset)&
            &bind(C, name="sddk_get_gvec_offset_fft")
            integer,                 intent(in)  :: gvec_id
            integer,                 intent(out) :: gvec_offset
        end subroutine

        subroutine sddk_fft(fft_id, dir, dat)&
            &bind(C, name="sddk_fft")
            integer,                 intent(in)  :: fft_id
            integer,                 intent(in)  :: dir
            complex(8),              intent(inout)  :: dat
        end subroutine

        subroutine sddk_fft_prepare(fft_id, gvec_id)&
            &bind(C, name="sddk_fft_prepare")
            integer,                 intent(in)  :: fft_id
            integer,                 intent(in)  :: gvec_id
        end subroutine

        subroutine sddk_fft_dismiss(fft_id)&
            &bind(C, name="sddk_fft_dismiss")
            integer,                 intent(in)  :: fft_id
        end subroutine

        subroutine sddk_print_timers()&
            &bind(C, name="sddk_print_timers")
        end subroutine

    end interface

contains

    function c_str(f_string) result(c_string)
        implicit none
        character(len=*), intent(in)  :: f_string
        character(len=1, kind=C_CHAR) :: c_string(len_trim(f_string) + 1)
        integer i
        do i = 1, len_trim(f_string)
            c_string(i) = f_string(i:i)
        end do
        c_string(len_trim(f_string) + 1) = C_NULL_CHAR
    end function c_str

end module
